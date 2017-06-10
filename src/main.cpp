#import "Arduino.h"
#include <Bounce2.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <OneWire.h>

// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library

#define ONE_WIRE_BUS 10
// Temperature high gisteresis
byte TH = 26;
// Temperature low gisteresis
byte TL = 25;

// 0 TL setting
// 1 TH setting
// 2 normal
int mode = 0;
unsigned long setupModeLastInteraction = 0;

const int pButton = A0;
const int pRotaryA = A1;
const int pRotaryB = A2;

#define INTERACTION_TIME 4000

const int pRelay = 11;

Bounce buttonDebouncer = Bounce();
Bounce rotaryDebouncer = Bounce();

OneWire oneWire(ONE_WIRE_BUS); // on pin (a 4.7K resistor is necessary)
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

int displayPins[] = {7, 8, 2, 5, 6, 3, 4};
byte displaySymbols[] = {
    B00010000, //-
    B11111100, B01100000, B11011010, B11110010, B01100110,
    B10110110, B10111110, B11100000, B11111110, B11110110,
    B10000000, //+
};

void dispTemp(int temp) {
  int num = temp - 20;
  num = constrain(num, -1, 10);
  num = num + 1; // because our array starter from 0 and have shifting
  int mask = 128;
  for (int i = 0; i < 7; i++) {
    if ((mask & displaySymbols[num]) == 0)
      digitalWrite(displayPins[i], LOW);
    else
      digitalWrite(displayPins[i], HIGH);
    mask = mask >> 1;
  }
}

void animateStart() {
  for (int i = 0; i < 6; i++) {
    digitalWrite(displayPins[i], HIGH);
    delay(50);
  }
}

void dispOff() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(displayPins[i], LOW);
  }
}

void dispModeTL() {  
  dispTemp(19);
  delay(500);
}

void dispModeTH() {
  dispTemp(30);
  delay(500);
}

void dispMode() {
  switch (mode) {
  case 0:
    dispModeTL();
    dispTemp(TL);    
    break;
  case 1:
    dispModeTH();
    dispTemp(TH);    
    break;
  case 2:
    dispOff();
    delay(50);
    animateStart();
    break;
  default:
    dispOff();
  }
}

int nextMode() { 
  Serial.print("current mode ");
  Serial.print(mode);
  Serial.print(" changed to ");
  mode = ++mode % 3; 
  Serial.println(mode);
  return mode;
}

void changeGisteresisLevel(int mode, int delta) {
  int temp = 0;
  if (mode)
    temp = TH = constrain(TH + delta,19,30);
  else
    temp = TL = constrain(TL + delta,19,30);
  EEPROM.write(mode, temp);
  dispTemp(temp);
}

byte readTL() {
  TL = EEPROM.read(0);
  if (TL == 255) {
    EEPROM.write(0, 24);
  }
  Serial.print("TL=");
  Serial.println(TL);
  return TL;
}

byte readTH() {
  TH = EEPROM.read(1);
  if (TH == 255) {
    EEPROM.write(1, 25);
  }
  Serial.print("TH=");
  Serial.println(TH);
  return TH;
}

void setup(void) {
  Serial.begin(9600);
  sensors.begin();

  for (int i = 0; i < 7; i++) {
    pinMode(displayPins[i], OUTPUT);
  }
  pinMode(pRelay, OUTPUT);
  pinMode(pButton, INPUT_PULLUP);
  pinMode(pRotaryA, INPUT_PULLUP);
  pinMode(pRotaryB, INPUT_PULLUP);

  buttonDebouncer.attach(pButton);
  buttonDebouncer.interval(5);

  rotaryDebouncer.attach(pRotaryA);
  rotaryDebouncer.interval(1);

  TL = readTL();
  TH = readTH();  

  //show boot and TL TH values
  for(int i = 0; i<=3; i++){
    mode = i;
    dispMode();
    delay(800);
  }  
  mode = 2;
}

#define TEMP_REQ_FREQUENCY_TIME  4000
unsigned long requestedTemperatureLastTime = 0;

void loop(void) {
  buttonDebouncer.update();
  if (buttonDebouncer.rose()) {
    Serial.println("Button pressed");
    nextMode();
    dispMode();
    setupModeLastInteraction = millis();
  }
  if (mode < 2) {
    if (millis() - setupModeLastInteraction > INTERACTION_TIME) {
      // autoexit from setup menu
      setupModeLastInteraction = 0;
      mode = 2;
      dispMode();
      return;
    }
    rotaryDebouncer.update();
    if (rotaryDebouncer.rose()) {
      setupModeLastInteraction = millis();
      if (digitalRead(pRotaryB))
        changeGisteresisLevel(mode, 1);
      else
        changeGisteresisLevel(mode, -1);
    }
    return;
  }
  if (millis() - requestedTemperatureLastTime < TEMP_REQ_FREQUENCY_TIME)
    return;
  sensors.requestTemperatures(); // Send the command to get temperatures
  requestedTemperatureLastTime = millis();  
  int celsius = sensors.getTempCByIndex(0);
  Serial.print(celsius);
  Serial.println("C");

  if (celsius < TL) {
    digitalWrite(pRelay, HIGH);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  if (celsius > TH) {
    digitalWrite(pRelay, LOW);
    digitalWrite(LED_BUILTIN, LOW);
  }
  dispTemp(celsius);
}
