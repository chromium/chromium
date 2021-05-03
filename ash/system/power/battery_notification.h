// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BATTERY_NOTIFICATION_H_
#define ASH_SYSTEM_POWER_BATTERY_NOTIFICATION_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_notification_controller.h"
#include "base/macros.h"

namespace message_center {
class MessageCenter;
}

namespace ash {

// Class for showing and hiding a MessageCenter low battery notification.
class ASH_EXPORT BatteryNotification {
 public:
  BatteryNotification(
      message_center::MessageCenter* message_center,
      PowerNotificationController::NotificationState notification_state);
  ~BatteryNotification();

  static const char kNotificationId[];

  // Updates the notification if it still exists.
  void Update(
      PowerNotificationController::NotificationState notification_state);

 private:
  message_center::MessageCenter* message_center_;

  DISALLOW_COPY_AND_ASSIGN(BatteryNotification);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BATTERY_NOTIFICATION_H_
