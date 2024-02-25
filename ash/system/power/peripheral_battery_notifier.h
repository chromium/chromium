// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_NOTIFIER_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_NOTIFIER_H_

#include <cstdint>
#include <map>
#include <optional>

#include "ash/ash_export.h"
#include "ash/system/power/peripheral_battery_listener.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {

class BluetoothDevice;
class PeripheralBatteryNotifierTest;

// This class listens for peripheral device battery status and shows
// notifications for low battery conditions.
class ASH_EXPORT PeripheralBatteryNotifier
    : public PeripheralBatteryListener::Observer {
 public:
  static const char kStylusNotificationId[];

  // This class registers/unregisters itself as an observer in ctor/dtor.
  explicit PeripheralBatteryNotifier(PeripheralBatteryListener* listener);
  PeripheralBatteryNotifier(const PeripheralBatteryNotifier&) = delete;
  PeripheralBatteryNotifier& operator=(const PeripheralBatteryNotifier&) =
      delete;
  ~PeripheralBatteryNotifier() override;

 private:
  friend class PeripheralBatteryNotifierListenerTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest,
                           InvalidBatteryInfo);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest,
                           ExtractBluetoothAddress);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest, DeviceRemove);

  friend class PeripheralBatteryNotifierTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierTest, EarlyNotification);

  struct NotificationInfo {
    NotificationInfo();
    NotificationInfo(std::optional<uint8_t> level,
                     base::TimeTicks last_notification_timestamp);
    ~NotificationInfo();
    NotificationInfo(const NotificationInfo& info);
    // Battery level within range [0, 100].
    std::optional<uint8_t> level;
    base::TimeTicks last_notification_timestamp;
    bool ever_notified;
  };

  // PeripheralBatteryListener::Observer:
  void OnAddingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnRemovingBattery(
      const PeripheralBatteryListener::BatteryInfo& battery) override;
  void OnUpdatedBatteryLevel(
      const PeripheralBatteryListener::BatteryInfo& battery) override;

  // Updates the battery information of the peripheral, and calls to post a
  // notification if the battery level is under the threshold.
  void UpdateBattery(const PeripheralBatteryListener::BatteryInfo& battery);

  // Updates the battery percentage in the corresponding notification.
  void UpdateBatteryNotificationIfVisible(
      const PeripheralBatteryListener::BatteryInfo& battery);

  // Calls to display a notification only if kNotificationInterval seconds have
  // passed since the last notification showed, avoiding the case where the
  // battery level oscillates around the threshold level.
  void ShowNotification(const PeripheralBatteryListener::BatteryInfo& battery);

  // Posts a low battery notification. If a notification
  // with the same id exists, its content gets updated.
  void ShowOrUpdateNotification(
      const PeripheralBatteryListener::BatteryInfo& battery);

  void CancelNotification(
      const PeripheralBatteryListener::BatteryInfo& battery_info);

  // Record of existing battery notification information, keyed by keys
  // provided by PeripheralBatteryListener.
  std::map<std::string, NotificationInfo> battery_notifications_;

  raw_ptr<PeripheralBatteryListener> peripheral_battery_listener_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_NOTIFIER_H_
