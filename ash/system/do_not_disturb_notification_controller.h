// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DO_NOT_DISTURB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_DO_NOT_DISTURB_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

// Controller class to manage the "Do not disturb" notification.
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

  // Gets the singleton instance that lives within `Shell` if available.
  static DoNotDisturbNotificationController* Get();

  // message_center::MessageCenterObserver:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // This is called by `FocusModeController::ExtendActiveSessionDuration` to
  // update the do not disturb notification with the latest end time.
  void MaybeUpdateNotification();
};

}  // namespace ash

#endif  // ASH_SYSTEM_DO_NOT_DISTURB_NOTIFICATION_CONTROLLER_H_
