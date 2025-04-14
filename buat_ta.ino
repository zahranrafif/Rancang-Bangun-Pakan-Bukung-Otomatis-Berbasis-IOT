#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h> 

const char *ssid = "Kuntilan@k";   
const char *password = "Zalfa13d53f"; 
const char *ntpServer = "asia.pool.ntp.org";
const long  gmtOffset_sec = 7 * 3600;

// const char *apiServo = "http://192.168.100.144/pakanburung/api-servo.php";
// const char *apiServoHapus = "http://192.168.100.144/pakanburung/deleteServo.php"; 
// const char *infrared = "http://192.168.100.144/pakanburung/infrared.php"; 
// const char *gettime = "http://192.168.100.144/pakanburung/gettime.php";
// const char *servoMove = "http://192.168.100.144/pakanburung/servoMove.php";

const char *apiServo = "http://birdu.kchb.org/api-servo.php";
const char *apiServoHapus = "http://birdu.kchb.org/deleteServo.php";
const char *infrared = "http://birdu.kchb.org/infrared.php";
const char *gettime = "http://birdu.kchb.org/gettime.php"; 
const char *servoMove = "http://birdu.kchb.org/servoMove.php";

#define TRIG_PIN 23
#define ECHO_PIN 22 
#define IR_PIN 25 
const int NUM_READINGS = 10;
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
#define BUZZER_PIN 13

int servoPin = 2;
Servo servo;
int duration_us; 
long distance_cm;

String lastFeedingTime = "";
String lastFeedingDate = "";

WiFiServer server(80);

unsigned long previousMillis = 0; 
const long interval = 30000;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec);

void setup() {
  Serial.begin(115200);
  connectWiFi();

  servo.attach(servoPin);
  servo.write(40);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(IR_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  dht.begin();

  timeClient.begin();

  server.begin();
  Serial.println("Server Telah Dimulai.");
}

void connectWiFi(){
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");

  // tunggu koneksi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to "), Serial.println(ssid);
  Serial.print("IP address: "), Serial.println(WiFi.localIP());
}

void loop() {
  moveServo();
  time();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    delay(2000);
    readDistance();
    suhu();
    readInfrared();
  }
  delay(1000);
}

void time() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
    Serial.println("NTP Time: " + timeClient.getFormattedTime());

    String currentDateString = getCurrentDateString();
    if (currentDateString != lastFeedingDate) {
      lastFeedingTime = "";
      lastFeedingDate = currentDateString;
    }

    String feedingTimesJson = getFeedingTime(); 
    if (feedingTimesJson != "") {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, feedingTimesJson);
      JsonArray feedingTimes = doc.as<JsonArray>();

      for (String feedingTime : feedingTimes) {
        if (isTimeForFeeding(feedingTime)) { 
          moveScheduleServo();
          lastFeedingTime = feedingTime; 
          delay(60000); 
          break;
        }
      }
    }

    delay(1000);
  } else {
    Serial.println("WiFi is not connected. Please check your WiFi connection.");
    delay(5000);
  }
}

String getFeedingTime() {
  HTTPClient http;
  http.begin(gettime);
  int httpResponseCode = http.GET();
  if (httpResponseCode == HTTP_CODE_OK) {
    String feedingTimes = http.getString();
    return feedingTimes;
  } else {
    Serial.printf("HTTP GET request failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    return ""; 
  }
}

bool isTimeForFeeding(String feedingTime) {
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  int feedingHour = feedingTime.substring(0, 2).toInt();
  int feedingMinute = feedingTime.substring(3, 5).toInt();
  
  return (currentHour == feedingHour && currentMinute == feedingMinute && feedingTime != lastFeedingTime);
}

String getCurrentDateString() {
  time_t rawtime = timeClient.getEpochTime();
  struct tm* timeinfo = localtime(&rawtime);
  char buffer[11];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", timeinfo);
  return String(buffer);
}

void moveScheduleServo() {
      for (int i = 40; i <= 90; i++) {
          servo.write(i);
          delay(5);
        }
        delay(200);

        for (int i = 90; i >= 40; i--) {
          servo.write(i);
          delay(5);
        }
        servo.write(40);
        updateStatusSuccess();
        delay(60000);
}

void moveServo() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(apiServo);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload);

      // masih dicoba coba
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      int status = doc["status"];

      if (status == 1) {
        for (int i = 40; i <= 90; i++) {
          servo.write(i);
          delay(5);
        }
        delay(200);

        for (int i = 90; i >= 40; i--) {
          servo.write(i);
          delay(5);
        }

        servo.write(40);
        updateStatusSuccess();

        hapusStatus1();
      } else if (status == 0) {
        setStatusZero();
      }
    }
    http.end();
  }
  delay(1000);
}

void hapusStatus1() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(apiServoHapus);

    // Creating JSON payload
    DynamicJsonDocument doc(1024);
    doc["status"] = 1;
    String json;
    serializeJson(doc, json);

    int httpCode = http.sendRequest("DELETE", json);

     if (httpCode > 0) {
        if (httpCode == 200) {
            Serial.println(" ");
            Serial.println("Status 1 deleted from database");
        } else {
            Serial.println("Tidak ada data, Response status code: " + String(httpCode));
        }
    } else {
        Serial.println("Error in HTTP request");
    }
    http.end();
  }
  delay(1000);
}

void setStatusZero() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(apiServo);
    int httpCode = http.GET();

    if (httpCode > 0) {
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, http.getString());

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
      }

      if (doc["success"]) {
        Serial.println("Status set to 0 in database");
      } else {
        Serial.println("");
      }
    }
    http.end();
  }
  delay(1000);
}

void updateStatusSuccess() {
  HTTPClient http;

  http.begin(servoMove);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  int httpCode = http.POST("status=success_moved");
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Status berhasil di kirim: " + payload);
  } else {
    Serial.println("Gagal update status");
  }
  http.end();
  delay(1000);
}

void suhu(){
  float suhu = dht.readTemperature();
  int kelembaban = dht.readHumidity();

  Serial.println("Suhu : " + String(suhu,1));
  Serial.println("Kelembaban : " + String(kelembaban));
  
  HTTPClient http;
  // String url = "http://192.168.100.144/pakanburung/suhukelembaban.php?suhu=" + String(suhu) + "&kelembaban=" + String(kelembaban);
  String url = "http://birdu.kchb.org/suhukelembaban.php?suhu=" + String(suhu,1) + "&kelembaban=" + String(kelembaban);

  http.begin(url);

  String respon = http.getString();
  Serial.println(respon);
  
  int httpResponseCode = http.GET();
  
  // Mengecek status respons
  if (httpResponseCode > 0) {
    Serial.print("SUHU HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.println("SUHU Error sending GET request");
  }
  http.end();

  if (suhu >= 32.0) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
  delay(1000);
}

void readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  duration_us = pulseIn(ECHO_PIN, HIGH);
  
  distance_cm = (duration_us*0.034/2) / 1.03;

  Serial.print("Jarak Pakan: ");
  Serial.print(distance_cm);
  Serial.println(" cm");

  sendDistanceToServer(distance_cm);
  delay(3000);
}

void sendDistanceToServer(float distance) {
  // String url = "http://192.168.100.144/pakanburung/jarak.php?distance=" + String(distance);
  String url = "http://birdu.kchb.org/jarak.php?distance=" + String(distance);

  HTTPClient http; 
  http.begin(url);
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    Serial.print("JARAK HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.println("JARAK Error sending GET request");
  }
  http.end();
}

void readInfrared(){
  int irValue = digitalRead(IR_PIN);

  if (irValue == LOW) {
    Serial.println("Terdapat Pakan Dalam Wadah.");
    updateStatusInfrared(1);
  } else {
    Serial.println("Wadah kosong.");
    updateStatusInfrared(0);
  }
  delay(1000);
}

void updateStatusInfrared(int status) {
  HTTPClient http;

  http.begin(infrared);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  // Membuat payload untuk dikirim ke server
  String postData = "status=" + String(status);

  int httpResponseCode = http.POST(postData);

  if (httpResponseCode > 0) {
    Serial.print("Status berhasil dikirim. Respons server: " + String(http.getString()));
  } else {
    Serial.print("Gagal mengirim status. Kode respons: ");
    Serial.println(httpResponseCode);
  }
    Serial.println(" ");
  http.end();
}


