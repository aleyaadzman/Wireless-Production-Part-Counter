// ✅ ESP32 Conveyor Counter with Blynk, Telegram & Google Form Integration

#define BLYNK_TEMPLATE_ID "TMPL6dlHGAuOF"
#define BLYNK_TEMPLATE_NAME "Wireless Production Parts Counter"
#define BLYNK_AUTH_TOKEN "7qwICXqqS7cI4yH620vxCWRnm_rq9_si"

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp32.h>

// 🧾 Google Form Fields
const char* formURL = "https://docs.google.com/forms/u/0/d/1gGaRecFKVtghxrbjVj_c3lFGrFYze-1XVdy_tMWjPr0/formResponse";
const char* entryProduct    = "entry.680203274";
const char* entryStatus     = "entry.83640785";
const char* entryUltrasonic = "entry.2122521285";

// 📱 Blynk Virtual Pins
#define VPIN_TARGET         V0
#define VPIN_GREEN_LED      V5
#define VPIN_RED_LED        V6
#define VPIN_SUCCESS_COUNT  V4
#define VPIN_FAIL_COUNT     V9

// 📍 I/O Pins
#define IR_SENSOR_PIN 2
#define TRIG_PIN      12
#define ECHO_PIN      14
#define GREEN_LED     4
#define RED_LED       18
#define BUZZER        5

// 🧠 Variables
int successCount = 0;
int failCount = 0;
int productTarget = 0;
bool targetReached = false;
bool isTargetSet = false;
int previousIRState = HIGH;

WiFiClientSecure secured_client;
UniversalTelegramBot bot("7933022830:AAEt_fI7jEsG2iR6Gdg7Dagy5XPQzGnl69w", secured_client);
const String chat_id = "848078487";

BlynkTimer timer;

// 🎯 Receive Target from Blynk
BLYNK_WRITE(VPIN_TARGET) {
  int newTarget = param.asInt();

  if (newTarget != productTarget || !isTargetSet) {
    productTarget = newTarget;
    successCount = 0;
    failCount = 0;
    targetReached = false;
    isTargetSet = true;

    Serial.print("🎯 New Target: ");
    Serial.println(productTarget);
    sendTelegramMessage("🎯 New Target Set: " + String(productTarget));

    Blynk.virtualWrite(VPIN_SUCCESS_COUNT, successCount);
    Blynk.virtualWrite(VPIN_FAIL_COUNT, failCount);
  }
}

// 📤 Send to Google Form
void sendToGoogleForm(String product, String status, String ultrasonic) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String postData = String(entryProduct) + "=" + product + "&" +
                      entryStatus + "=" + status + "&" +
                      entryUltrasonic + "=" + ultrasonic;

    http.begin(secured_client, formURL);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    int code = http.POST(postData);
    Serial.print("Form response: ");
    Serial.println(code);
    http.end();
  }
}

// ✉️ Send Telegram Notification
void sendTelegramMessage(String msg) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(chat_id, msg, "");
  }
}

// 📏 Ultrasonic Measurement
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration * 0.034 / 2;
}

// ✅ Verify Object in Box
bool detectObjectPassing() {
  const int minThreshold = 2;  // cm
  const int maxThreshold = 6;  // cm
  int valid = 0;
  for (int i = 0; i < 6; i++) {
    long distance = measureDistance();
    if (distance >= minThreshold && distance <= maxThreshold) valid++;
    delay(50);
  }
  return (valid >= 3);
}

// 🔄 Monitor IR + Ultrasonic Sensor
void monitorSensors() {
  int irState = digitalRead(IR_SENSOR_PIN);

  if (irState == LOW && previousIRState == HIGH && !targetReached && isTargetSet) {
    delay(3000); // wait for product to settle in box

    if (detectObjectPassing()) {
      successCount++;
      String label = "Product " + String(successCount);

      // ✅ SUCCESS - Green LED + Long Beep
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      Blynk.virtualWrite(VPIN_GREEN_LED, 255);
      Blynk.virtualWrite(VPIN_RED_LED, 0);

      digitalWrite(BUZZER, HIGH); delay(1000); digitalWrite(BUZZER, LOW);

      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
      Blynk.virtualWrite(VPIN_GREEN_LED, 0);
      Blynk.virtualWrite(VPIN_RED_LED, 255);

      Serial.println("✅ Counted: " + label);
      sendTelegramMessage("📦 " + label + " detected & ✅ Passed!");
      sendToGoogleForm(label, "Detected", "✅ Passed");

      Blynk.virtualWrite(VPIN_SUCCESS_COUNT, successCount);

      if (successCount >= productTarget) {
        targetReached = true;
        sendTelegramMessage("🎯 Target of " + String(productTarget) + " reached!");
        Serial.println("🎉 Target reached!");
      }

    } else {
      failCount++;
      Blynk.virtualWrite(VPIN_FAIL_COUNT, failCount);

      // ❌ FAILURE - Red LED + Double Short Beep
      digitalWrite(BUZZER, HIGH); delay(150); digitalWrite(BUZZER, LOW); delay(100);
      digitalWrite(BUZZER, HIGH); delay(150); digitalWrite(BUZZER, LOW);

      Serial.println("❌ Product failed.");
      sendTelegramMessage("⚠️ Product failed ultrasonic check ❌ Missed.");
      sendToGoogleForm("Product ?", "Detected", "❌ Missed");
    }
  }

  previousIRState = irState;
}

// 🛠️ Setup
void setup() {
  Serial.begin(115200);
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  digitalWrite(BUZZER, LOW);

  secured_client.setInsecure();

  WiFiManager wm;
  if (!wm.autoConnect("ESP32_Setup")) {
    Serial.println("❌ WiFi Failed. Restarting...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("✅ WiFi Connected!");
  sendTelegramMessage("✅ ESP32 Connected to WiFi.");

  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  timer.setInterval(500L, monitorSensors);
}

// 🔁 Loop
void loop() {
  Blynk.run();
  timer.run();
}
