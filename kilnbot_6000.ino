
#include <SPI.h>
#include <EEPROM.h>
#include "Adafruit_MAX31855.h"

#define MAXDO   10
#define MAXCS   15
#define MAXCLK  14

#define SSR 9
#define ACT_LED 13

#define HYST 10

// eeprom address for storing max temp setting
#define MAXTEMP_ADDR  8

// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);
double ambientTemp;
double c;
uint16_t maxTemp;
bool ssrState;


void setup() {
  Serial.begin(115200);
  while (!Serial) delay(1);
  Serial.println(F("====================="));
  Serial.println(F("    Kilnbot 6000"));
  Serial.println(F("====================="));
  // wait for MAX chip to stabilize
  delay(500);
  thermocouple.setErrorMask(0x2);

  // set relay and indicator LED pins to
  // outputs, and drive them low
  pinMode(SSR, OUTPUT);
  pinMode(ACT_LED, OUTPUT);
  digitalWrite(SSR, LOW);
  digitalWrite(ACT_LED, LOW);
  Serial.setTimeout(2000); // for slow typers
  // load the saved max temp setting from eeprom
  EEPROM.get( MAXTEMP_ADDR, maxTemp );
}

void loop() {
  boolean fault = 0;
  String faultDescription;

  ambientTemp = thermocouple.readInternal();
  c = thermocouple.readCelsius();
  // did we get a valid temperature from the
  // thermocouple?
  if (isnan(c)) {
    // ... nope, try and diagnose the problem
    fault = true;
    uint8_t e = thermocouple.readError();
    if (e) {
      if (e & MAX31855_ERR_OC) {
        faultDescription = "[open circuit] ";
      }
      if (e & MAX31855_ERR_GND) {
        faultDescription += "[short to GND] ";
      }
      if (e & MAX31855_ERR_VCC) {
        faultDescription += "[short to VCC]";
      }
      if (faultDescription) {
        Serial.print("Fault: ");
        Serial.println(faultDescription);
      }
    }
  }
  // are we over temperature?
  else if (c > maxTemp) {
    // yes, switch off the kiln
    digitalWrite(SSR, LOW);
  }
  /*
    if the kiln *should* be powered on, but
    it's below maxtemp (minus hysterisis value),
    we're either still warming up, or have cooled down
    from an over-temp condition. Either way, switch on
    the kiln
  */
  else if ((c < (maxTemp - HYST)) && (ssrState == 1)) {
    digitalWrite(SSR, HIGH);
  }

  delay(250);
  // check to see if we've been sent a command
  checkSerial();
}


void checkSerial(void) {
  char input [32];
  String inputString = "";
  char cmd [10];
  char action [2];
  char val [8];
  bool wmode = false;
  inputString.reserve(30);
  if (Serial.available() > 0) {
    memset(action, 0, sizeof(action));
    memset(val, 0, sizeof(val));
    memset(cmd, 0, sizeof(cmd));

    inputString += Serial.readStringUntil(13);
    inputString.toCharArray(input, 30);
    uint8_t objs = sscanf(input, "AT+%8[A-Z]%[=?]%4s", &cmd, &action, &val);
    if (objs < 2) {
      // we need at least a command and an action to do anything...
      return;
    }
    // enable this for terminal echo
    // Serial.println(input);

    // ? is a read request, = is a write request
    if (strcmp(action, "?") == 0) {
      wmode = false;
    }
    else if (strcmp(action, "=") == 0) {
      wmode = true;
    }
  }


  if (strcmp(cmd, "TEMP") == 0) {
    Serial.print(F("TEMP=")); Serial.println(c);
  }
  else if (strcmp(cmd, "AMB") == 0) {
    Serial.print(F("AMBIENT=")); Serial.println(ambientTemp);
  }

  else if (strcmp(cmd, "STATE") == 0) {
    if (wmode) {
      if (atoi(val) > 0) {
        digitalWrite(SSR, HIGH);
        ssrState = 1;
      }
      else {
        digitalWrite(SSR, LOW);
        ssrState = 0;
      }
      Serial.println(F("OK"));
    }
    else {
      Serial.print(F("STATE="));
      //Serial.println(digitalRead(SSR));
      Serial.println(ssrState);
    }
  }
  else if (strcmp(cmd, "MAXTEMP") == 0) {
    if (wmode) {
      maxTemp = atoi(val);
      EEPROM.put( MAXTEMP_ADDR, maxTemp );
      Serial.println("OK");
    }
    else {
      Serial.print("MAXTEMP="); Serial.println(maxTemp);
    }
  }
}


bool inRange(int val, int minimum, int maximum)
{
  return ((minimum <= val) && (val <= maximum));
}

