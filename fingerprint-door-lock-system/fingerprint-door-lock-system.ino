/*
 * Fingerprint Door Lock System
 * 
 * Designed for Novita Palilati's Final Project
 * 
 * The circuit:
 * Using DFRobot LCD Keypad Shield
 * Fingerprint Sensor: TX to D2; RX to D3; Vcc to 3.3V;
 * Passive Buzzer to A1 pin
 * Solenoid driver to A2 pin (active low)
 * 
 * ADC Value for Analog Keypad:
 * Key RIGHT  = 0
 * Key UP     = 131
 * Key DOWN   = 306
 * Key LEFT   = 479
 * Key SELECT = 720
 * 
 * 
 * Created 2 May 2018
 * @Gorontalo, Indonesia
 * by ZulNs
 */

#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>

#define LCD_RS 8
#define LCD_EN 9
#define LCD_D4 4
#define LCD_D5 5
#define LCD_D6 6
#define LCD_D7 7

#define SS_RX 2
#define SS_TX 3

#define KEY_NONE   0
#define KEY_RIGHT  1
#define KEY_UP     2
#define KEY_DOWN   3
#define KEY_LEFT   4
#define KEY_SELECT 5

#define MENU_ENROLL         0
#define MENU_DELETE         1
#define MENU_EMPTY_DATABASE 2
#define MENU_TEMPLATE_COUNT 3
#define MENU_CHANGE_PIN     4
#define MENU_LCD_TIMEOUT    5
#define MENU_KEY_TIMEOUT    6
#define MENU_RETURN         7

const uint16_t EEPROM_PIN = EEPROM.length() - 7; // Max 6 chrs + zero chr
const uint16_t EEPROM_LCD_TIMEOUT = EEPROM.length() - 11; // Max 6 chrs + zero chr
const uint16_t EEPROM_KEY_TIMEOUT = EEPROM.length() - 15; // Max 6 chrs + zero chr

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
SoftwareSerial mySerial(SS_RX, SS_TX);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

char pin[7] = "908576";
char keyBuffer[7];
uint8_t oldKey;
uint32_t lcdTimeout, keyTimeout, lcdTimer, keyTimer;

void setup()  
{
  finger.begin(57600);
  lcd.begin(16, 2);
  lcd.print(F("  FINGERPRINT"));
  bitSet(DDRB, 2);   // Set D10 as output
  bitSet(PORTB, 2);  // Set D10 high (LCD backlight on)
  bitSet(DDRC, 2);   // Set A2 as output
  bitSet(PORTC, 2);  // Set A2 high (Solenoid Driver off)
  delay(2000);
  
  EEPROM.get(EEPROM_PIN, pin);
  EEPROM.get(EEPROM_LCD_TIMEOUT, lcdTimeout);
  EEPROM.get(EEPROM_KEY_TIMEOUT, keyTimeout);

  lcdPrintL1(F("Init fingerprint"));
  while (!finger.verifyPassword());
  lcdPrintL2(F("Tmpl count: "));
  finger.getTemplateCount();
  lcd.print(finger.templateCount);
  delay(2000);
  
  oldKey = getKey();
  printWaiting();
  enableLcdTimeout();
}

void loop()
{
  uint8_t id, key = getKey();
  id = getFingerprintId();
  if (id > 0)
  {
    printWaiting();
  }
  isLcdTimeout();
  if (key != oldKey)
  {
    if (oldKey == KEY_NONE)
    {
      soundTick();
      enableLcdTimeout();
      if (key == KEY_SELECT)
      {
        keyTimer = lcdTimer;
        if (getPin())
        {
          mainMenu();
        }
        enableLcdTimeout();
        printWaiting();
      }
    }
    oldKey = key;
  }
}

// returns 0 if failed, otherwise returns ID #
uint8_t getFingerprintId()
{
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)
  {
    return 0;
  }
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)
  {
    return 0;
  }
  enableLcdTimeout();
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)
  {
    lcdPrintL1(F("Unrecognized"));
  }
  else
  {
    lcdPrintL1(F("Found ID #"));
    lcd.print(finger.fingerID);
    lcdPrintL2(F("Confidence: "));
    lcd.print(finger.confidence);
    delay(2000);
    bitClear(PORTC, 2);  // Set A2 low (Solenoid Driver on)
    lcdPrintL2(F("Box unlocked.."));
    delay(20000);
    bitSet(PORTC, 2);  // Set A2 high (Solenoid Driver off)
    lcdPrintL2(F("Box locked.."));
    delay(2000);
  }
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  return (p == FINGERPRINT_OK) ? finger.fingerID : 255;
}

uint8_t getFingerprintEnroll(const uint8_t id)
{
  uint8_t p;
  lcdPrintL1(F("Finger #"));
  lcd.print(id);
  p = fingerGetImage();
  if (p != FINGERPRINT_OK)
  {
    return p;
  }
  p = fingerImage2Tz(1);
  if (p != FINGERPRINT_OK)
  {
    return p;
  }
  lcdPrintL2(F("Remove finger"));
  while (finger.getImage() != FINGERPRINT_NOFINGER);
  lcdPrintL1(F("Finger again"));
  p = fingerGetImage();
  if (p != FINGERPRINT_OK)
  {
    return p;
  }
  p = fingerImage2Tz(2);
  if (p != FINGERPRINT_OK)
  {
    return p;
  }
  lcdPrintL1(F("Creating model"));
  p = finger.createModel();
  if (p == FINGERPRINT_OK)
  {
    lcdPrintL2(F("Prints matched!"));
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    lcdPrintL2(F("Comm error"));
  }
  else if (p == FINGERPRINT_ENROLLMISMATCH)
  {
    lcdPrintL2(F("Did not match"));
  }
  else
  {
    lcdPrintL2(F("Unknown error"));
  }
  delay(2000);
  if (p != FINGERPRINT_OK)
  {
    return p;
  }
  lcdPrintL1(F("Storing model"));
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    lcdPrintL2(F("Stored #"));
    lcd.print(id);
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    lcdPrintL2(F("Comm error"));
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    lcdPrintL2(F("Could not store"));
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    lcdPrintL2(F("Error flashing"));
  }
  else
  {
    lcdPrintL2(F("Unknown error"));
  }
  delay(2000);
  return p;   
}

uint8_t fingerGetImage()
{
  uint8_t p;
  while (true)
  {
    p = finger.getImage();
    switch (p)
    {
      case FINGERPRINT_OK:
        lcdPrintL2(F("Image taken"));
        break;
      case FINGERPRINT_NOFINGER:
        if (isKeyTimeout())
        {
          return -1;
        }
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        lcdPrintL2(F("Comm error"));
        break;
      case FINGERPRINT_IMAGEFAIL:
        lcdPrintL2(F("Imaging error"));
        break;
      default:
        lcdPrintL2(F("Unknown error"));
    }
    if (p != FINGERPRINT_NOFINGER)
    {
      delay(2000);
      keyTimer = millis();
      return p;
    }
  }
}

uint8_t fingerImage2Tz(const uint8_t slot)
{
  uint8_t p = finger.image2Tz(slot);
  switch (p)
  {
    case FINGERPRINT_OK:
      lcdPrintL1(F("Image converted"));
      break;
    case FINGERPRINT_IMAGEMESS:
      lcdPrintL1(F("Image too messy"));
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcdPrintL1(F("Comm error"));
      break;
    case FINGERPRINT_FEATUREFAIL:
      lcdPrintL1(F("Features fail"));
      break;
    case FINGERPRINT_INVALIDIMAGE:
      lcdPrintL1(F("Invalid image"));
      break;
    default:
      lcdPrintL1(F("Unknown error"));
  }
  delay(2000);
  return p;
}

uint8_t deleteFingerprint(const uint8_t id)
{
  uint8_t p = finger.deleteModel(id);
  if (p == FINGERPRINT_OK)
  {
    lcdPrintL1(F("Deleted #"));
    lcd.print(id);
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    lcdPrintL1(F("Comm error"));
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    lcdPrintL1(F("Bad location"));
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    lcdPrintL1(F("Flashing error"));
  }
  else
  {
    lcdPrintL1(F("Unknown error"));
  }
  delay(2000);
  return p;   
}

void mainMenu()
{
  uint8_t key, menu = MENU_ENROLL;
  char chrBuffer[7];
  boolean isYes;
  lcdPrintL1(F("Select:"));
  while (true)
  {
    switch (menu)
    {
      case MENU_ENROLL:
        lcdPrintL2(F("Enroll"));
        break;
      case MENU_DELETE:
        lcdPrintL2(F("Delete"));
        break;
      case MENU_EMPTY_DATABASE:
        lcdPrintL2(F("Empty database"));
        break;
      case MENU_TEMPLATE_COUNT:
        lcdPrintL2(F("Template count"));
        break;
      case MENU_CHANGE_PIN:
        lcdPrintL2(F("Change PIN"));
        break;
      case MENU_LCD_TIMEOUT:
        lcdPrintL2(F("LCD timeout"));
        break;
      case MENU_KEY_TIMEOUT:
        lcdPrintL2(F("Key timeout"));
        break;
      case MENU_RETURN:
        lcdPrintL2(F("Return"));
    }
    key = getAKey();
    switch (key)
    {
      case KEY_DOWN:
        menu++;
        if (menu == MENU_RETURN + 1)
        {
          menu = MENU_ENROLL;
        }
        break;
      case KEY_UP:
        menu--;
        if (menu == 255)  // 255 is rollover value after decrement of 0
        {
          menu = MENU_RETURN;
        }
        break;
      case KEY_SELECT:
        switch (menu)
        {
          case MENU_ENROLL:
            if (!enrollOrDelete(true))
            {
              return;
            }
            break;
          case MENU_DELETE:
            if (!enrollOrDelete(false))
            {
              return;
            }
            break;
          case MENU_EMPTY_DATABASE:
            key = getSureness();
            if (key == 255)
            {
              return;
            }
            else if (key == 0)
            {
              finger.emptyDatabase();
              lcdPrintL1(F("Now database is"));
              lcdPrintL2(F("empty"));
              delay(2000);
            }
            break;
          case MENU_TEMPLATE_COUNT:
            finger.getTemplateCount();
            lcdPrintL1(F("Template: "));
            lcd.print(finger.templateCount);
            delay(2000);
            break;
          case MENU_CHANGE_PIN:
            if (!getNewPin(F("New PIN:"), 9))
            {
              return;
            }
            if (keyBuffer[0] != 0)
            {
              strcpy(chrBuffer, keyBuffer);
              if (!getNewPin(F("Confirm:"), 9))
              {
                return;
              }
              if (keyBuffer[0] != 0)
              {
                if (strcmp(chrBuffer, keyBuffer) == 0)
                {
                  strcpy(pin, keyBuffer);
                  savePin();
                  lcdPrintL1(F("PIN changed"));
                }
                else
                {
                  lcdPrintL1(F("PIN unchanged"));
                }
                delay(2000);
              }
            }
            break;
          case MENU_LCD_TIMEOUT:
            if (!getTimeout(F("LCD T/O: "), 9, true))
            {
              return;
            }
            break;
          case MENU_KEY_TIMEOUT:
            if (!getTimeout(F("Key T/O: "), 9, false))
            {
              return;
            }
            break;
          case MENU_RETURN:
            return;
        }
        lcdPrintL1(F("Select:"));
        keyTimer = millis();
        break;
      case 255:
        return;
    }
  }
}

boolean enrollOrDelete(const boolean isEnroll)
{
  uint8_t id;
  while (true)
  {
    if (!getNumber(F("ID#"), 3, 6, false))
    {
      return false;
    }
    id = atoi(keyBuffer);
    if (id > 127)
    {
      lcdPrintL1(F("0 < ID < 128"));
      if (getAKey() == 255)
      {
        return false;
      }
      continue;
    }
    if (id == 0)
    {
      return true;
    }
    if (isEnroll)
    {
      if (getFingerprintEnroll(id) == 255)
      {
        return false;
      }
    }
    else
    {
      deleteFingerprint(id);
    }
    keyTimer = millis();
  }
}

uint8_t getSureness()
{
  uint8_t key, pos = 8;
  lcd.cursor();
  lcd.blink();
  lcdPrintL1(F("Sure? Y/N"));
  while (true)
  {
    lcd.setCursor(pos, 0);
    key = getAKey();
    switch (key)
    {
      case KEY_LEFT:
        pos = 6;
        break;
      case KEY_RIGHT:
        pos = 8;
        break;
      default:
        lcd.noCursor();
        lcd.noBlink();
        return (key == 255 ? -1 : pos == 6 ? 0 : 1);
    }
  }
}

boolean getTimeout(const __FlashStringHelper *flashStr, const uint8_t pos, const boolean isLcd)
{
  uint16_t to = isLcd ? lcdTimeout / 1000 : keyTimeout / 1000;  // convert milliseconds to seconds
  uint16_t newTo;
  while (true)
  {
    itoa(to, keyBuffer, 10);
    if (!getNumber(flashStr, pos, 6, true))
    {
      return false;
    }
    newTo = atoi(keyBuffer);
    if (keyBuffer[0] == 0 || isLcd && newTo * 1000 == lcdTimeout || !isLcd && newTo * 1000 == keyTimeout)
    {
      lcdPrintL1(F("T/O unchanged"));
      delay(2000);
      return true;
    }
    if (9 < newTo && newTo < 3601)
    {
      break;
    }
    lcdPrintL1(F("9 < T/O < 3601"));
    if (getAKey() == 255)
    {
      return false;
    }
  }
  if (isLcd)
  {
    lcdTimeout = newTo * 1000;
    EEPROM.put(EEPROM_LCD_TIMEOUT, lcdTimeout);
    lcdPrintL1(F("LCD"));
  }
  else
  {
    keyTimeout = newTo * 1000;
    EEPROM.put(EEPROM_KEY_TIMEOUT, keyTimeout);
    lcdPrintL1(F("Key"));
  }
  lcd.print(F(" T/O changed"));
  delay(2000);
  return true;
}

boolean getNewPin(const __FlashStringHelper *flashStr, const uint8_t pos)
{
  uint8_t len;
  do
  {
    if (!getNumber(flashStr, pos, 6, false))
    {
      return false;
    }
    len = strlen(keyBuffer);
  }
  while (0 < len && len < 4);
  return true;
}

boolean getPin()
{
  return getNumber(F("PIN:"), 5, 6, false) && (strcmp(pin, keyBuffer) == 0);
}

boolean getNumber(const __FlashStringHelper *flashStr, const uint8_t pos, const uint8_t len, const boolean preVal)
{
  uint8_t srcCtr = 6, destCtr = preVal ? strlen(keyBuffer) : 0, key;
  char chr;
  lcdPrintL1(F("      0123456789"));
  lcdPrintL2(flashStr);
  if (preVal)
  {
    lcd.print(keyBuffer);
  }
  lcd.setCursor(srcCtr, 0);
  lcd.cursor();
  lcd.blink();
  while (true)
  {
    key = getAKey();
    switch (key)
    {
      case KEY_RIGHT:
        srcCtr++;
        if (srcCtr == 16)
        {
          srcCtr = 6;
        }
        lcd.setCursor(srcCtr, 0);
        break;
      case KEY_LEFT:
        srcCtr--;
        if (srcCtr == 5)
        {
          srcCtr = 15;
        }
        lcd.setCursor(srcCtr, 0);
        break;
      case KEY_DOWN:
        chr = 42 + srcCtr;
        if (destCtr == len)
        {
          destCtr--;
        }
        keyBuffer[destCtr] = chr;
        lcd.setCursor(pos + destCtr, 1);
        lcd.print(chr);
        lcd.setCursor(srcCtr, 0);
        destCtr++;
        break;
      case KEY_UP:
        if (destCtr > 0)
        {
          destCtr--;
          lcd.setCursor(pos + destCtr, 1);
          lcd.print(F(" "));
          lcd.setCursor(srcCtr, 0);
        }
        break;
      case KEY_SELECT:
        keyBuffer[destCtr] = 0;
        lcd.noCursor();
        lcd.noBlink();
        return true;
      case 255:
        return false;
    }
  }
}

uint8_t getAKey()
{
  uint8_t key, key0 = getKey();
  while (true)
  {
    if (isKeyTimeout())
    {
      return -1;
    }
    key = getKey();
    if (key != key0)
    {
      if (key0 == KEY_NONE)
      {
        soundTick();
        enableLcdTimeout();
        keyTimer = lcdTimer;
        return key;
      }
      key0 = key;
    }
  }
}

uint8_t getKey()
{
  int16_t adc0, adc;
  do
  {
    adc0 = analogRead(0);
    delay(10);
    adc = analogRead(0);
  }
  while (abs(adc - adc0) > 10);
  if (adc < 50)
  {
    return KEY_RIGHT;
  }
  else if (adc < 181)
  {
    return KEY_UP;
  }
  else if (adc < 356)
  {
    return KEY_DOWN;
  }
  else if (adc < 524)
  {
    return KEY_LEFT;
  }
  else if (adc < 770)
  {
    return KEY_SELECT;
  }
  return KEY_NONE;
}

void soundTick()
{
  tone(A1, 4000, 25);
}

void printWaiting()
{
  lcdPrintL1(F("Waiting 4 finger"));
}

void lcdPrintL1(const __FlashStringHelper *flashStr)
{
  lcd.clear();
  lcd.print(flashStr);
}

void lcdPrintL2(const __FlashStringHelper *flashStr)
{
  lcd.setCursor(0, 1);
  lcd.print(F("                "));
  lcd.setCursor(0, 1);
  lcd.print(flashStr);
}

void enableLcdTimeout()
{
  bitSet(PORTB, 2);
  lcdTimer = millis();
}

void isLcdTimeout()
{
  if (millis() - lcdTimer >= lcdTimeout)
  {
    bitClear(PORTB, 2);
  }
}

boolean isKeyTimeout()
{
  return (millis() - keyTimer >= keyTimeout);
}

void savePin()
{
  uint8_t ctr = 0, chr;
  do
  {
    chr = pin[ctr];
    EEPROM.update(EEPROM_PIN + ctr, chr);
    ctr++;
  }
  while (chr != 0);
}

