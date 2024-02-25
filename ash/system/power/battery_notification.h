// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BATTERY_NOTIFICATION_H_
#define ASH_SYSTEM_POWER_BATTERY_NOTIFICATION_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_notification_controller.h"
#include "base/memory/raw_ptr.h"

namespace message_center {
class MessageCenter;
}

namespace ash {

// Class for showing and hiding a MessageCenter low battery notification.
class ASH_EXPORT BatteryNotification {
 public:
  BatteryNotification(
      message_center::MessageCenter* message_center,
      PowerNotificationController* power_notification_controller);

  BatteryNotification(const BatteryNotification&) = delete;
  BatteryNotification& operator=(const BatteryNotification&) = delete;

  ~BatteryNotification();

  static const char kNotificationId[];

  // Updates the notification if it still exists.
  void Update();

 private:
  raw_ptr<message_center::MessageCenter> message_center_;
  raw_ptr<PowerNotificationController> power_notification_controller_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BATTERY_NOTIFICATION_H_
