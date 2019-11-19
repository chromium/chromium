// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_NOTIFIER_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_NOTIFIER_H_

#include <cstdint>
#include <map>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {

class BluetoothDevice;
class PeripheralBatteryNotifierTest;

// This class listens for peripheral device battery status and shows
// notifications for low battery conditions.
class ASH_EXPORT PeripheralBatteryNotifier
    : public chromeos::PowerManagerClient::Observer,
      public device::BluetoothAdapter::Observer {
 public:
  static const char kStylusNotificationId[];

  // This class registers/unregisters itself as an observer in ctor/dtor.
  PeripheralBatteryNotifier();
  ~PeripheralBatteryNotifier() override;


  // chromeos::PowerManagerClient::Observer:
  void PeripheralBatteryStatusReceived(const std::string& path,
                                       const std::string& name,
                                       int level) override;

  // device::BluetoothAdapter::Observer:
  void DeviceBatteryChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothDevice* device,
      base::Optional<uint8_t> new_battery_percentage) override;
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  friend class PeripheralBatteryNotifierTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierTest, InvalidBatteryInfo);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierTest,
                           ExtractBluetoothAddress);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierTest, DeviceRemove);

  struct BatteryInfo {
    BatteryInfo();
    BatteryInfo(const base::string16& name,
                base::Optional<uint8_t> level,
                base::TimeTicks last_notification_timestamp,
                bool is_stylus,
                const std::string& bluetooth_address);
    ~BatteryInfo();
    BatteryInfo(const BatteryInfo& info);

    // Human readable name for the device. It is changeable.
    base::string16 name;
    // Battery level within range [0, 100].
    base::Optional<uint8_t> level;
    base::TimeTicks last_notification_timestamp;
    bool is_stylus = false;
    // Peripheral's Bluetooth address. Empty for non-Bluetooth devices.
    std::string bluetooth_address;
  };

  void InitializeOnBluetoothReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Removes the Bluetooth battery with address |bluetooth_address|, as well as
  // the associated notification. Called when a bluetooth device has been
  // changed or removed.
  void RemoveBluetoothBattery(const std::string& bluetooth_address);

  // Updates the battery information of the peripheral with the corresponding
  // |map_key|, and calls to post a notification if the battery level is under
  // the threshold.
  void UpdateBattery(const std::string& map_key,
                     const BatteryInfo& battery_info);

  // Updates the battery percentage in the corresponding notification.
  void UpdateBatteryNotificationIfVisible(const std::string& map_key,
                                          const BatteryInfo& battery);

  // Calls to display a notification only if kNotificationInterval seconds have
  // passed since the last notification showed, avoiding the case where the
  // battery level oscillates around the threshold level.
  void ShowNotification(const std::string& map_key, const BatteryInfo& battery);

  // Posts a low battery notification with id as |map_key|. If a notification
  // with the same id exists, its content gets updated.
  void ShowOrUpdateNotification(const std::string& map_key,
                                const BatteryInfo& battery);

  void CancelNotification(const std::string& map_key);

  // Record of existing battery information. For Bluetooth Devices, the key is
  // kBluetoothDeviceIdPrefix + the device's address. For HID devices, the key
  // is the device path. If a device uses HID over Bluetooth, it is indexed as a
  // Bluetooth device.
  std::map<std::string, BatteryInfo> batteries_;

  // PeripheralBatteryNotifier is an observer of |bluetooth_adapter_| for
  // bluetooth device change/remove events.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  const base::TickClock* clock_;

  std::unique_ptr<base::WeakPtrFactory<PeripheralBatteryNotifier>>
      weakptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_NOTIFIER_H_
