// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_

#include "ash/system/power/peripheral_battery_listener.h"
#include "chromeos/dbus/power/power_manager_client.h"

// Constants common to peripheral battery listener and notifier tests.

namespace {

// HID device.
constexpr char kTestBatteryPath[] =
    "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery";
constexpr char kTestBatteryAddress[] = "aa:bb:cc:dd:ee:ff";
constexpr char kTestDeviceName[] = "test device";
constexpr char16_t kTestDeviceName16[] = u"test device";
const auto kTestBatteryStatusIn = power_manager::
    PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
const auto kTestBatteryStatusOut =
    ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging;
constexpr char kTestBatteryId[] = "battery_bluetooth-aa:bb:cc:dd:ee:ff";
constexpr char kTestBatteryNotificationId[] =
    "battery_notification-battery_bluetooth-aa:bb:cc:dd:ee:ff";

// Charging device
constexpr char kTestChargerPath[] = "/sys/class/power_supply/PCHG0";
constexpr char kTestChargerName[] = "";
constexpr char kTestChargerId[] = "PCHG0";
constexpr char kTestOtherChargerPath[] = "/sys/class/power_supply/PCHG1";
constexpr char kTestOtherChargerName[] = "";
constexpr char kTestOtherChargerId[] = "PCHG1";

// Bluetooth devices.
constexpr char kBluetoothDeviceAddress1[] = "aa:bb:cc:dd:ee:ff";
constexpr char kBluetoothDeviceAddress2[] = "11:22:33:44:55:66";
constexpr char kBluetoothDeviceName1[] = "device_name_1";
constexpr char16_t kBluetoothDeviceName116[] = u"device_name_1";
constexpr char kBluetoothDeviceName2[] = "device_name_2";
constexpr char16_t kBluetoothDeviceName216[] = u"device_name_2";
constexpr char kBluetoothDeviceId1[] = "battery_bluetooth-aa:bb:cc:dd:ee:ff";
constexpr char kBluetoothDeviceNotificationId1[] =
    "battery_notification-battery_bluetooth-aa:bb:cc:dd:ee:ff";
constexpr char kBluetoothDeviceId2[] = "battery_bluetooth-11:22:33:44:55:66";
constexpr char kBluetoothDeviceNotificationId2[] =
    "battery_notification-battery_bluetooth-11:22:33:44:55:66";

// Stylus devices.
const std::string kTestStylusBatteryPath =
    "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
const std::string kTestStylusName = "test_stylus";
const auto kTestStylusBatteryStatusDischargingIn = power_manager::
    PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
const auto kTestStylusBatteryStatusDischargingOut =
    ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging;
// A period of time less than full garage charge, in seconds
const int kPartialGarageChargeTime = 3;
// A period of time greater than full garage charge, in seconds
const int kFullGarageChargeTime = 30;

constexpr char kStylusChargerDeviceName[] = "garaged-stylus-charger";

}  // namespace

// Provide pretty-printers in aid of EXPECT_CALL() diagnostics.
namespace absl {

inline void PrintTo(const absl::optional<uint8_t>& optional, std::ostream* os) {
  if (!optional.has_value()) {
    *os << "absl::nullopt";
  } else {
    *os << (int)*optional;
  }
}

}  // namespace absl

namespace ash {

inline void PrintTo(
    const ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus& status,
    std::ostream* os) {
  switch (status) {
    case ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kUnknown:
      *os << "Unknown";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::
        kDischarging:
      *os << "Discharging";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kCharging:
      *os << "Charging";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kFull:
      *os << "Full";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::
        kNotCharging:
      *os << "NotCharging";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kError:
      *os << "Error";
      break;
    default:
      *os << "unknown-enum-value";
  }
  *os << "(" << (int)status << ")";
}

inline void PrintTo(
    const ash::PeripheralBatteryListener::BatteryInfo::PeripheralType& type,
    std::ostream* os) {
  switch (type) {
    case ash::PeripheralBatteryListener::BatteryInfo::PeripheralType::kOther:
      *os << "Other";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::PeripheralType::
        kStylusViaScreen:
      *os << "StylusViaScreen";
      break;
    case ash::PeripheralBatteryListener::BatteryInfo::PeripheralType::
        kStylusViaCharger:
      *os << "StylusViaCharger";
      break;
    default:
      *os << "unknown-enum-value";
  }
  *os << "(" << (int)type << ")";
}

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_
