// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DO_NOT_DISTURB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_DO_NOT_DISTURB_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// Controller class to manage the "Do not disturb" notification. This class only
// exists when `IsQsRevampEnabled` is true.
class ASH_EXPORT DoNotDisturbNotificationController
    : public message_center::MessageCenterObserver {
 public:
  DoNotDisturbNotificationController();

  DoNotDisturbNotificationController(
      const DoNotDisturbNotificationController&) = delete;
  DoNotDisturbNotificationController& operator=(
      const DoNotDisturbNotificationController&) = delete;

  ~DoNotDisturbNotificationController() override;

  static const char kDoNotDisturbNotificationId[];

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

 private:
  std::unique_ptr<message_center::Notification> CreateNotification();
};

}  // namespace ash

#endif  // ASH_SYSTEM_DO_NOT_DISTURB_NOTIFICATION_CONTROLLER_H_
