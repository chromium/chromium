// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_

// Constants common to peripheral battery listener and notifier tests.

namespace {

// HID device.
constexpr char kTestBatteryPath[] =
    "/sys/class/power_supply/hid-AA:BB:CC:DD:EE:FF-battery";
constexpr char kTestBatteryAddress[] = "aa:bb:cc:dd:ee:ff";
constexpr char kTestDeviceName[] = "test device";
constexpr char kTestBatteryId[] = "battery_bluetooth-aa:bb:cc:dd:ee:ff";
constexpr char kTestBatteryNotificationId[] =
    "battery_notification-battery_bluetooth-aa:bb:cc:dd:ee:ff";

// Bluetooth devices.
constexpr char kBluetoothDeviceAddress1[] = "aa:bb:cc:dd:ee:ff";
constexpr char kBluetoothDeviceAddress2[] = "11:22:33:44:55:66";
constexpr char kBluetoothDeviceName1[] = "device_name_1";
constexpr char kBluetoothDeviceName2[] = "device_name_2";
constexpr char kBluetoothDeviceId1[] = "battery_bluetooth-aa:bb:cc:dd:ee:ff";
constexpr char kBluetoothDeviceNotificationId1[] =
    "battery_notification-battery_bluetooth-aa:bb:cc:dd:ee:ff";
constexpr char kBluetoothDeviceId2[] = "battery_bluetooth-11:22:33:44:55:66";
constexpr char kBluetoothDeviceNotificationId2[] =
    "battery_notification-battery_bluetooth-11:22:33:44:55:66";

}  // namespace

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_TESTS_H_
