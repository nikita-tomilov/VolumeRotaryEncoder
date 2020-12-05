#include "GyverEncoder.h"
//https://github.com/AlexGyver/GyverLibs#GyverEncoder
#include "HID-Project.h"
//https://github.com/NicoHood/HID/wiki/Consumer-API

#include <Adafruit_NeoPixel.h> 
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include <EEPROM.h>

Encoder enc1(9,10,8,true);
Encoder enc2(6,5,7,true);
Encoder enc3(3,2,4,true);
Adafruit_NeoPixel led(1, A3, NEO_GRB + NEO_KHZ800);

#define LAYER_DEFAULT 0
#define LAYER_FSIGNS  1
#define LAYER_SERIAL  2
#define LAYER_LIGHTROOM  3
#define LAYER_MIXED  4

#define LAYER_DEFAULT_COLOR 0,5,5
#define LAYER_FSIGNS_COLOR 5,5,0
#define LAYER_SERIAL_COLOR 5,0,5
#define LAYER_LIGHTROOM_COLOR 0,0,5
#define LAYER_MIXED_COLOR 0,5,0

#define LIGHT_LAYER 0
#define LIGHT_RGB 1
#define LIGHT_OFF 2

#define LAYER_ADDRESS 42
#define LIGHT_MODE_ADDRESS 54 

unsigned long ledBlinkStartTs = 0;
unsigned long ledRGBLastChangeTs = 0;
int8_t layer = 0;
int8_t lightMode = 0;
float hue = 0.0;

float fract(float x) { return x - int(x); }

float mix(float a, float b, float t) { return a + (b - a) * t; }

float step(float e, float x) { return x < e ? 0.0 : 1.0; }

float* hsv2rgb(float h, float s, float b, float* rgb) {
  rgb[0] = b * mix(1.0, constrain(abs(fract(h + 1.0) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  rgb[1] = b * mix(1.0, constrain(abs(fract(h + 0.6666666) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  rgb[2] = b * mix(1.0, constrain(abs(fract(h + 0.3333333) * 6.0 - 3.0) - 1.0, 0.0, 1.0), s);
  return rgb;
}

int8_t store(int address, int8_t val) {
  Serial.print("store "); Serial.print(address); Serial.print(" "); Serial.println((int)val);
  EEPROM.update(address, val);
  return val;
}

int8_t retrieve(int address) {
  int8_t val = EEPROM.read(address);
  Serial.print("retrieve "); Serial.print(address); Serial.print(": "); Serial.println((int)val);
  return val;
}

void sled(byte r, byte g, byte b) {
  led.setPixelColor(0, led.Color(r,g,b));
  led.show();
}

void ledRGBifNeeded() {
  if (ledBlinkStartTs != 0) return;
  if (lightMode != LIGHT_RGB) return;
  if (millis() - ledRGBLastChangeTs < 5) return;
  hue += 0.001;
  if (hue > 1) hue = 0;
  float rgb[3];
  hsv2rgb(hue, 1.0, 1.0, rgb);
  float r = rgb[0] * 255.0;
  float g = rgb[1] * 255.0;
  float b = rgb[2] * 255.0;
  float avg = (r + g + b) / 3;
  float k = (256.0 / 3) / avg;
  r = r * k;
  g = g * k;
  b = b * k;
  ledRGBLastChangeTs = millis();
  sled((byte)r,(byte)g,(byte)b);
}

void ledIndicateLayer() {
  if (lightMode == LIGHT_LAYER) {
    if (layer == LAYER_DEFAULT) sled(LAYER_DEFAULT_COLOR);
    if (layer == LAYER_FSIGNS) sled(LAYER_FSIGNS_COLOR);
    if (layer == LAYER_SERIAL) sled(LAYER_SERIAL_COLOR);
    if (layer == LAYER_LIGHTROOM) sled(LAYER_LIGHTROOM_COLOR);
    if (layer == LAYER_MIXED) sled(LAYER_MIXED_COLOR);
  }
  if (lightMode == LIGHT_OFF) {
    sled(0,0,0);
  } 
}

void ledBlinkStart() {
  ledBlinkStartTs = millis();
  sled(10, 10, 10);
}

void ledBlinkLedChangeStart() {
  ledBlinkStartTs = millis();
  if (layer == LAYER_DEFAULT) sled(LAYER_DEFAULT_COLOR);
  if (layer == LAYER_FSIGNS) sled(LAYER_FSIGNS_COLOR);
  if (layer == LAYER_SERIAL) sled(LAYER_SERIAL_COLOR);
  if (layer == LAYER_LIGHTROOM) sled(LAYER_LIGHTROOM_COLOR);
  if (layer == LAYER_MIXED) sled(LAYER_MIXED_COLOR);
}

void incrementLayer() {
  layer += 1;
  if (layer > LAYER_MIXED) {
    layer = 0; 
  }
  ledIndicateLayer();
  ledBlinkLedChangeStart();
  store(LAYER_ADDRESS, layer);
}

void decrementLayer() {
  layer -= 1;
  if (layer < 0) {
    layer = LAYER_MIXED; 
  }
  ledIndicateLayer();
  ledBlinkLedChangeStart();
  store(LAYER_ADDRESS, layer);
}

void changeLightMode() {
  lightMode += 1;
  if (lightMode > LIGHT_OFF) {
    lightMode = 0; 
  }
  ledIndicateLayer();
  store(LIGHT_MODE_ADDRESS, lightMode);
}

void ledBlinkFinish() {
  if (ledBlinkStartTs == 0) return;
  if (millis() - ledBlinkStartTs > 50) {
    ledIndicateLayer();
    ledBlinkStartTs = 0;
  }
}

void enc1TickLeft() {
  if (layer == LAYER_DEFAULT) Consumer.write(MEDIA_VOLUME_DOWN);
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F13));
  if (layer == LAYER_SERIAL) Serial.println("1L");
  if (layer == LAYER_LIGHTROOM) BootKeyboard.write(KeyboardKeycode(KEY_COMMA));
  if (layer == LAYER_MIXED) Consumer.write(MEDIA_VOLUME_DOWN);
  ledBlinkStart();
}

void enc1TickRight() {
  if (layer == LAYER_DEFAULT) Consumer.write(MEDIA_VOLUME_UP);
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F14));
  if (layer == LAYER_SERIAL) Serial.println("1R");
  if (layer == LAYER_LIGHTROOM) BootKeyboard.write(KeyboardKeycode(KEY_PERIOD));
  if (layer == LAYER_MIXED) Consumer.write(MEDIA_VOLUME_UP);
  ledBlinkStart();
}

void enc1TickPress() {
  if (layer == LAYER_DEFAULT) Consumer.write(MEDIA_PLAY_PAUSE);
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F15));
  if (layer == LAYER_SERIAL) Serial.println("1P");
  if (layer == LAYER_LIGHTROOM) {
    BootKeyboard.press(KeyboardKeycode(KEY_LEFT_CTRL));
    BootKeyboard.write(KeyboardKeycode(KEY_Z));
    BootKeyboard.release(KeyboardKeycode(KEY_LEFT_CTRL));
  }
  if (layer == LAYER_MIXED) Consumer.write(MEDIA_PLAY_PAUSE);
  ledBlinkStart();
}

void enc1TickLeftHolded() {
  if (layer == LAYER_DEFAULT) Consumer.write(MEDIA_PREVIOUS);
  if (layer == LAYER_MIXED) Consumer.write(MEDIA_PREVIOUS);
  ledBlinkStart();
}

void enc1TickRightHolded() {
  if (layer == LAYER_DEFAULT) Consumer.write(MEDIA_NEXT);
  if (layer == LAYER_MIXED) Consumer.write(MEDIA_NEXT);
  ledBlinkStart();
}

void enc2TickLeft() {
  if (layer == LAYER_DEFAULT) BootKeyboard.write(KeyboardKeycode(KEY_UP));
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F16));
  if (layer == LAYER_SERIAL) Serial.println("2L");
  if (layer == LAYER_LIGHTROOM) BootKeyboard.write(KeyboardKeycode(KEY_MINUS));
  if (layer == LAYER_MIXED) BootKeyboard.write(KeyboardKeycode(KEY_UP));
  ledBlinkStart();
}

void enc2TickRight() {
  if (layer == LAYER_DEFAULT) BootKeyboard.write(KeyboardKeycode(KEY_DOWN));
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F17));
  if (layer == LAYER_SERIAL) Serial.println("2R");
  if (layer == LAYER_LIGHTROOM) BootKeyboard.write(KeyboardKeycode(KEY_EQUAL));
  if (layer == LAYER_MIXED) BootKeyboard.write(KeyboardKeycode(KEY_DOWN));
  ledBlinkStart();
}

void enc2TickPress() {
  if (layer == LAYER_DEFAULT) BootKeyboard.write(KeyboardKeycode(KEY_ENTER));
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F18));
  if (layer == LAYER_SERIAL) Serial.println("2P");
  if (layer == LAYER_MIXED) BootKeyboard.write(KeyboardKeycode(KEY_ENTER));
  ledBlinkStart();
}

void enc3TickLeft() {
  if (layer == LAYER_DEFAULT) BootKeyboard.write(KeyboardKeycode(KEY_LEFT));
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F19));
  if (layer == LAYER_SERIAL) Serial.println("3L");
  if (layer == LAYER_LIGHTROOM) BootKeyboard.write(KeyboardKeycode(KEY_LEFT));
  if (layer == LAYER_MIXED) BootKeyboard.write(KeyboardKeycode(KEY_F13));
  ledBlinkStart();
}

void enc3TickRight() {
  if (layer == LAYER_DEFAULT) BootKeyboard.write(KeyboardKeycode(KEY_RIGHT));
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F20));
  if (layer == LAYER_SERIAL) Serial.println("3R");
  if (layer == LAYER_LIGHTROOM) BootKeyboard.write(KeyboardKeycode(KEY_RIGHT));
  if (layer == LAYER_MIXED) BootKeyboard.write(KeyboardKeycode(KEY_F15));
  ledBlinkStart();
}

void enc3TickPress() {
  if (layer == LAYER_DEFAULT) BootKeyboard.write(KeyboardKeycode(KEY_SPACE));
  if (layer == LAYER_FSIGNS) BootKeyboard.write(KeyboardKeycode(KEY_F21));
  if (layer == LAYER_SERIAL) Serial.println("3P");
  if (layer == LAYER_LIGHTROOM) {
    BootKeyboard.press(KeyboardKeycode(KEY_LEFT_CTRL));
    BootKeyboard.write(KeyboardKeycode(KEY_Y));
    BootKeyboard.release(KeyboardKeycode(KEY_LEFT_CTRL));
  }
  if (layer == LAYER_MIXED) BootKeyboard.write(KeyboardKeycode(KEY_F14));
  ledBlinkStart();
}

void setup() {
  layer = retrieve(LAYER_ADDRESS);
  lightMode = retrieve(LIGHT_MODE_ADDRESS);
  Serial.begin(115200);
  led.begin();
  BootKeyboard.begin();
  sled(150,0,0);
  delay(100);
  sled(150,150,0);
  delay(100);
  sled(0,150,0);
  delay(100);
  sled(0,150,150);
  delay(100);
  sled(0,0,150);
  delay(100);
  ledIndicateLayer();
}

void loop() {
  enc1.tick();
  enc2.tick();
  enc3.tick();
  ledBlinkFinish();
  ledRGBifNeeded();
  
  if (enc1.isLeft())   enc1TickLeft();
  if (enc1.isRight())  enc1TickRight();
  if (enc1.isLeftH())  enc1TickLeftHolded();
  if (enc1.isRightH()) enc1TickRightHolded();
  if (enc1.isRelease())  enc1TickPress();

  if (enc2.isLeft())   enc2TickLeft();
  if (enc2.isRight())  enc2TickRight();
  if (enc2.isLeftH())  decrementLayer();
  if (enc2.isRightH()) incrementLayer(); 
  if (enc2.isHolded()) changeLightMode();
  if (enc2.isRelease())enc2TickPress();

  if (enc3.isLeft())   enc3TickLeft();
  if (enc3.isRight())  enc3TickRight();
  if (enc3.isPress())  enc3TickPress();
}
