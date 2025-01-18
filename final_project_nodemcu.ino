// include library WiFi dan database
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
// include library LCD, RTC, dan sensor
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>

// define sensor
#define tempWire D4
OneWire oneWire(tempWire);
DallasTemperature sensor(&oneWire);
#define sensorLDR D0
#define sensorHujan D7
#define sensorTanah D5
#define pinAnalog A0 
// define relay
#define relayPompa D8
#define relaySolenoid1 D1
#define relaySolenoid2 D3
#define relaySolenoid3 D6
// define WiFi
#define WIFI_SSID "Tselhome-4BE3"
#define WIFI_PASSWPRD "********"

// define database
#define API_KEY "***************************************"
#define DATABASE_URL "nyiram-app-default-rtdb.************************************"
#define DATABASE_SECRET "****************************************"
// define auth untuk Android app
#define USER_EMAIL "botani@nyiram.org"
#define USER_PASSWORD "***************"

// variable LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);
// variable RTC
RTC_DS1307 rtc;

// variable solenoid
const int solenoid1Duration = 60000 * 5;  
const int solenoid2Duration = 60000 * 4;
const int solenoid3Duration = 60000 * 1;

// char 7 hari dalam seminggu menggunakan array of arrays
char daysOfTheWeek[7][12] = {
  "Minggu",
  "Senin",
  "Selasa",
  "Rabu",
  "Kamis",
  "Jumat",
  "Sabtu"
};

// variable suhu
int suhu;
// variable cahaya
String statusCahaya;
int cahaya;
// variable hujan
int readHujan;
int nilaiHujan;
String statusHujan;
// variable tanah
int readTanah;
int nilaiTanah;
int persenTanah;
// variable pompa
String statusPompa;

// variable data ketika diambil dari Firebase 
String pumpStatus;
int Temp;
String Rainfall;
String Intensity;
int jam;
int menit;
int RH;

// class dan variable database
FirebaseData fbdo, fbdo_pump;
FirebaseAuth auth;
FirebaseConfig config;

// variable waktu pengiriman data
unsigned long sendDataPrevMillis = 0;

// variable sensor hujan untuk membaca curah hujan
int raindrop() {
  digitalWrite(sensorHujan, HIGH);
  digitalWrite(sensorTanah, LOW);
  delay(100);
  return analogRead(pinAnalog);
}

// variable sensor tanah untuk membaca kelembapan tanah
int soilmoisture() {
  digitalWrite(sensorTanah, HIGH);
  digitalWrite(sensorHujan, LOW);
  delay(100);
  return analogRead(pinAnalog);
}

// function setup
void setup() {
  // atur serial monitor
  Serial.begin(115200);
  // atur pinMode
  pinMode(sensorLDR, INPUT);
  pinMode(sensorHujan, OUTPUT);
  pinMode(sensorTanah, OUTPUT);
  pinMode(relayPompa, OUTPUT);
  pinMode(relaySolenoid1, OUTPUT);
  pinMode(relaySolenoid2, OUTPUT);
  pinMode(relaySolenoid3, OUTPUT);
  // atur lcd
  lcd.init();
  lcd.backlight();
  delay(300);
  // atur sensor
  sensor.begin();
  // atur rtc
  rtc.begin();
  // atur WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting....");
    delay(300);
    lcd.setCursor(0, 0);
    lcd.print("Connecting..........");
    delay(300);
    lcd.clear();
  }
  // cetak serial monitor jika telah berhasil terkoneksi internet
  Serial.println();
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
  Serial.print("IP Address is: ");
  Serial.println(WiFi.localIP());
  // atur database dan autentifikasi
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  //stream data pompa
  if (!Firebase.RTDB.beginStream(&fbdo_pump, "Pump/Pompa"))
    Serial.printf("stream pump error, %s\n\n", fbdo_pump.errorReason().c_str());
}

// function loop
void loop() {
  if (Firebase.ready() && (millis() - sendDataPrevMillis > 300) || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();
    realtimeclock();
    temperature();
    rain();
    delay(100);
    soil();
    lit();

    // get data jam
    if (Firebase.RTDB.getInt(&fbdo, "RTC/Jam")) {
      if (fbdo.dataType() == "int") {
        jam = fbdo.intData();
        Serial.println(jam);
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    // get data menit
    if (Firebase.RTDB.getInt(&fbdo, "RTC/Menit")) {
      if (fbdo.dataType() == "int") {
        menit = fbdo.intData();
        Serial.println(menit);
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    // get data temp
    if (Firebase.RTDB.getInt(&fbdo, "DS18B20/Temperatur")) {
      if (fbdo.dataType() == "int") {
        Temp = fbdo.intData();
        Serial.println(Temp);
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    // get data intensity
    if (Firebase.RTDB.getString(&fbdo, "LDR/Cahaya")) {
      if (fbdo.dataType() == "string") {
        Intensity = fbdo.stringData();
        Serial.println(Intensity);
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    // get data rainfall
    if (Firebase.RTDB.getString(&fbdo, "Rain/Hujan")) {
      if (fbdo.dataType() == "string") {
        Rainfall = fbdo.stringData();
        Serial.println(Rainfall);
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    // get data RH
    if (Firebase.RTDB.getInt(&fbdo, "Tanah/Kelembapan")) {
      if (fbdo.dataType() == "int") {
        RH = fbdo.intData();
        Serial.println(RH);
      } else {
        Serial.println("FAILED: " + fbdo.errorReason());
      }
    }
    // get pumpstatus
    if (Firebase.ready()) {
      if (!Firebase.RTDB.readStream(&fbdo_pump))
        Serial.printf("stream 1 read error, %s\n\n", fbdo_pump.errorReason().c_str());
      if (fbdo_pump.streamAvailable()) {
        if (fbdo_pump.dataType() == "string") {
          pumpStatus = fbdo_pump.stringData();
          Serial.println(pumpStatus);
        }
      }
    }

    // pengaturan pompa
    if ((jam == 7 && menit == 0 && RH < 50) || (jam == 17 && menit == 0 && RH < 50))  // RH nya kurang nih
    {
      turnOnPumpAndSolenoidsAutomatic();
    } else if ((Temp >= 35) && (Rainfall == "YES") && (Intensity == "BRIGHT")) {
      turnOnPumpAndSolenoidsAutomatic();
    } else if (pumpStatus == "ON") {
      lcd.setCursor(0, 3);
      lcd.print("PUMP:  ");
      lcd.print(pumpStatus);
      turnOnPumpAndSolenoidsManual();
    } else {
      lcd.setCursor(0, 3);
      lcd.print("PUMP: ");
      lcd.print(pumpStatus);
      turnOffPumpAndSolenoids();
    }
  }
}

// function sensor tanah dengan keluaran analog
void soil() {
  // pembacaan sensor tanah
  readTanah = soilmoisture();
  // kalibrasi output analog sensor tanah
  nilaiTanah = readTanah + 124;
  // ubah hasil analog ke persentase
  persenTanah = (100 - ((nilaiTanah) / 1023) * 100);
  // cetak di serial monitor
  Serial.print("Analog Tanah: ");
  Serial.println(nilaiTanah);
  Serial.print("Persentase Tanah: ");
  Serial.println(persenTanah);
  // cetak di LCD
  lcd.setCursor(11, 1);
  lcd.print("SOIL:");
  lcd.print(persenTanah);
  lcd.print((char)37);
  // kirim data ke database
  Firebase.RTDB.setInt(&fbdo, "Tanah/Kelembapan", persenTanah);
  Firebase.RTDB.setInt(&fbdo, "Tanah/Nilai Tanah", nilaiTanah);
  Firebase.RTDB.setInt(&fbdo, "Tanah/Analog Tanah", readTanah);
}

// function sensor hujan dengan keluaran analog
void rain() {
  // pembacaan sensor hujan
  readHujan = raindrop();
  // kalibrasi output analog sensor hujan
  nilaiHujan = readHujan + 124;
  // logika sensor hujan
  if (nilaiHujan < 510) {
    statusHujan = "YES";
    // cetak di LCD
    lcd.setCursor(0, 2);
    lcd.print("RAIN:");
    lcd.print(statusHujan);
  } else {
    statusHujan = "NO";
    // cetak di LCD
    lcd.setCursor(0, 2);
    lcd.print("RAIN: ");
    lcd.print(statusHujan);
  }
  // cetak di serial monitor
  Serial.print("Hujan: ");
  Serial.println(statusHujan);
  Serial.print("Analog Hujan: ");
  Serial.println(nilaiHujan);
  // kirim data ke database
  Firebase.RTDB.setString(&fbdo, "Rain/Hujan", statusHujan);
  Firebase.RTDB.setInt(&fbdo, "Rain/Nilai", nilaiHujan);
  Firebase.RTDB.setInt(&fbdo, "Rain/Analog", readHujan);
}

// function sensor suhu
void temperature() {
  //pembacaan sensor suhu
  sensor.requestTemperatures();
  suhu = sensor.getTempCByIndex(0);
  // cetak di lcd
  lcd.setCursor(0, 1);
  lcd.print("TEMP");
  lcd.print(":");
  lcd.print(suhu);
  lcd.print((char)223);
  lcd.print("C");
  // cetak di serial monitor
  Serial.print("Temperatur: ");
  Serial.print(suhu);
  Serial.print(" °C");
  Serial.println();
  // kirim data ke database
  Firebase.RTDB.setInt(&fbdo, "DS18B20/Temperatur", suhu);
}

// function sensor LDR
void lit() {
  //function pembacaan sensor LDR
  cahaya = digitalRead(sensorLDR);
  if (cahaya == 0) {
    statusCahaya = "BRIGHT";
    // cetak di LCD
    lcd.setCursor(10, 2);
    lcd.print("LIT:");
    lcd.print(statusCahaya);
  } else {
    statusCahaya = "DARK";
    // cetak di LCD
    lcd.setCursor(10, 2);
    lcd.print("LIT:  ");
    lcd.print(statusCahaya);
  }

  // cetak di serial monitor
  Serial.print("Cahaya: ");
  Serial.println(statusCahaya);
  // kirim data ke database
  Firebase.RTDB.setString(&fbdo, "LDR/Cahaya", statusCahaya);
}

// function RTC
void realtimeclock() {
  DateTime now = rtc.now();
  String hari = daysOfTheWeek[now.dayOfTheWeek()];

  // cetak di serial monitor
  Serial.print("Current Date & Time: ");
  // cetak tanggal
  Serial.print(now.day(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.year(), DEC);
  // cetak hari
  Serial.print(" (");
  Serial.print(hari);
  Serial.print(") ");
  // cetak jam
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.println(now.minute(), DEC);

  // cetak di lcd
  // cetak hari
  lcd.setCursor(0, 0);
  lcd.print(hari.substring(0, 3));
  lcd.print("/");
  // cetak tanggal
  lcd.print(now.day(), DEC);
  lcd.print("/");
  lcd.print(now.month(), DEC);
  lcd.print("/");
  lcd.print(now.year(), DEC);
  // cetak jam
  lcd.setCursor(15, 0);
  lcd.print(now.hour());
  lcd.print(":");
  lcd.print(now.minute());

  // kirim data ke database
  Firebase.RTDB.setString(&fbdo, "RTC/Hari", hari);
  Firebase.RTDB.setInt(&fbdo, "RTC/Tanggal", now.day());
  Firebase.RTDB.setInt(&fbdo, "RTC/Bulan", now.month());
  Firebase.RTDB.setInt(&fbdo, "RTC/Tahun", now.year());
  Firebase.RTDB.setInt(&fbdo, "RTC/Jam", now.hour());
  Firebase.RTDB.setInt(&fbdo, "RTC/Menit", now.minute());
}

// function hidupkan pompa
void turnOnPump() {
  digitalWrite(relayPompa, HIGH);
  Serial.println("Pompa hidup");
}
// function matikan pompa
void turnOffPump() {
  digitalWrite(relayPompa, LOW);
  Serial.println("Pompa mati");
}
// function hidupkan solenoid 1
void turnOnSolenoid1() {
  digitalWrite(relaySolenoid1, HIGH);
  Serial.println("Solenoid 1 hidup");
}
// function matikan solenoid 1
void turnOffSolenoid1() {
  digitalWrite(relaySolenoid1, LOW);
  Serial.println("Solenoid 1 mati");
}
// function hidupkan solenoid 2
void turnOnSolenoid2() {
  digitalWrite(relaySolenoid2, HIGH);
  Serial.println("Solenoid 2 hidup");
}
// function matikan solenoid 2
void turnOffSolenoid2() {
  digitalWrite(relaySolenoid2, LOW);
  Serial.println("Solenoid 2 mati");
}
// function hidupkan solenoid 3
void turnOnSolenoid3() {
  digitalWrite(relaySolenoid3, HIGH);
  Serial.println("Solenoid 3 hidup");
}
// function matikan solenoid 3
void turnOffSolenoid3() {
  digitalWrite(relaySolenoid3, LOW);
  Serial.println("Solenoid 3 mati");
}

// function pengaturan pompa dan solenoid hidup otomatis
void turnOnPumpAndSolenoidsAutomatic() {
  statusPompa = "ON";
  Firebase.RTDB.setString(&fbdo, "Pump/Pompa", statusPompa);
  lcd.setCursor(0, 3);
  lcd.print("PUMP:  ");
  lcd.print(statusPompa);
  turnOnPump();
  turnOnSolenoid1();
  delay(solenoid1Duration);
  turnOnSolenoid2();
  delay(solenoid2Duration);
  turnOnSolenoid3();
  delay(solenoid3Duration);
  turnOffPumpAndSolenoids();
}

// function pengaturan untuk menghidupkan pompa secara manual dari app
void turnOnPumpAndSolenoidsManual() {
  pumpStatus = "ON";
  turnOnPump();
  turnOnSolenoid1();
  turnOnSolenoid2();
  turnOnSolenoid3();
}

// function pengaturan pompa dan solenoid mati otomatis
void turnOffPumpAndSolenoids() {
  pumpStatus = "OFF";
  Firebase.RTDB.setString(&fbdo, "Pump/Pompa", pumpStatus);
  turnOffPump();
  turnOffSolenoid1();
  turnOffSolenoid2();
  turnOffSolenoid3();
}