// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_CENTER_LOCK_SCREEN_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_CENTER_LOCK_SCREEN_CONTROLLER_H_

#include "ash/public/cpp/session/session_observer.h"
#include "components/account_id/account_id.h"
#include "ui/message_center/lock_screen/lock_screen_controller.h"

namespace ash {

// This class implements the ASH logic for the message center.
class AshMessageCenterLockScreenController
    : public message_center::LockScreenController,
      public SessionObserver {
 public:
  AshMessageCenterLockScreenController();

  AshMessageCenterLockScreenController(
      const AshMessageCenterLockScreenController&) = delete;
  AshMessageCenterLockScreenController& operator=(
      const AshMessageCenterLockScreenController&) = delete;

  ~AshMessageCenterLockScreenController() override;

  // message_center::LockScreenController:
  void DismissLockScreenThenExecute(base::OnceClosure pending_callback,
                                    base::OnceClosure cancel_callback,
                                    int message_id) override;
  bool IsScreenLocked() const override;
  bool IsNotificationAllowedOnLockScreen(const std::string& id) const override;

 private:
  // SessionObserver:
  void OnLockStateChanged(bool locked) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // Shows a message that asks a user to unlock the device.
  void EncourageUserToUnlock(int message_id);

  bool locked_;
  AccountId active_account_id_ = EmptyAccountId();
  base::OnceClosure pending_task_;
  base::OnceClosure cancel_task_;

  ScopedSessionObserver scoped_session_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_CENTER_LOCK_SCREEN_CONTROLLER_H_
