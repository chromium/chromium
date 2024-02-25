// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_

#include <optional>

#include "ash/system/power/peripheral_battery_listener.h"
#include "chromeos/dbus/power/power_manager_client.h"

// Constants common to peripheral battery listener and notifier tests.

// HID device.
inline constexpr char kTestBatteryPath[] =
    "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery";
inline constexpr char kTestBatteryAddress[] = "aa:bb:cc:dd:ee:ff";
inline constexpr char kTestDeviceName[] = "test device";
inline constexpr char16_t kTestDeviceName16[] = u"test device";
const inline auto kTestBatteryStatusIn = power_manager::
    PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
const inline auto kTestBatteryStatusOut =
    ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging;
inline constexpr char kTestBatteryId[] = "battery_bluetooth-aa:bb:cc:dd:ee:ff";
inline constexpr char kTestBatteryNotificationId[] =
    "battery_notification-battery_bluetooth-aa:bb:cc:dd:ee:ff";

// Charging device
inline constexpr char kTestChargerPath[] =
    "/sys/class/power_supply/peripheral0";
inline constexpr char kTestChargerName[] = "";
inline constexpr char kTestChargerId[] = "peripheral0";
inline constexpr char kTestOtherChargerPath[] =
    "/sys/class/power_supply/peripheral1";
inline constexpr char kTestOtherChargerName[] = "";
inline constexpr char kTestOtherChargerId[] = "peripheral1";
// TODO(b/215381232): Temporarily support both 'PCHG' name and 'peripheral' name
// till upstream kernel driver is merged. Remove variable when upstream kernel
// driver is merged.
inline constexpr char kTestPCHGChargerPath[] = "/sys/class/power_supply/PCHG0";

// Bluetooth devices.
inline constexpr char kBluetoothDeviceAddress1[] = "aa:bb:cc:dd:ee:ff";
inline constexpr char kBluetoothDeviceAddress2[] = "11:22:33:44:55:66";
inline constexpr char kBluetoothDeviceName1[] = "device_name_1";
inline constexpr char16_t kBluetoothDeviceName116[] = u"device_name_1";
inline constexpr char kBluetoothDeviceName2[] = "device_name_2";
inline constexpr char16_t kBluetoothDeviceName216[] = u"device_name_2";
inline constexpr char kBluetoothDeviceId1[] =
    "battery_bluetooth-aa:bb:cc:dd:ee:ff";
inline constexpr char kBluetoothDeviceNotificationId1[] =
    "battery_notification-battery_bluetooth-aa:bb:cc:dd:ee:ff";
inline constexpr char kBluetoothDeviceId2[] =
    "battery_bluetooth-11:22:33:44:55:66";
inline constexpr char kBluetoothDeviceNotificationId2[] =
    "battery_notification-battery_bluetooth-11:22:33:44:55:66";

// Stylus devices.
const inline char kTestStylusBatteryPath[] =
    "/sys/class/power_supply/hid-AAAA:BBBB:CCCC.DDDD-battery";
const inline char kTestStylusName[] = "test_stylus";
const inline auto kTestStylusBatteryStatusDischargingIn = power_manager::
    PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING;
const inline auto kTestStylusBatteryStatusDischargingOut =
    ash::PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging;
const inline char kStylusEligibleSerialNumbers[][18] = {
    "FABCDE01BCA23633", "019F02212D4F446E",  "154006440FE368C",
    "0190AB234FFE368",  "0154006440FE368C9", "0204009540fE368C9",
    "0347we-$%^$#^#*"};
const inline char kStylusIneligibleSerialNumbers[][17] = {
    "0190AB234FFE368C", "0190AB234fFe368C", "0154006440FE368C",
    "0204009540FE368C", "2011003140FE368C", ""};
// A period of time less than full garage charge, in seconds
const inline int kPartialGarageChargeTime = 3;
// A period of time greater than full garage charge, in seconds
const inline int kFullGarageChargeTime = 30;

inline constexpr char kStylusChargerDeviceName[] = "garaged-stylus-charger";

// Provide pretty-printers in aid of EXPECT_CALL() diagnostics.
namespace absl {

inline void PrintTo(const std::optional<uint8_t>& optional, std::ostream* os) {
  if (!optional.has_value()) {
    *os << "std::nullopt";
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
