// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_CENTER_LOCK_SCREEN_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_ASH_MESSAGE_CENTER_LOCK_SCREEN_CONTROLLER_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/gtest_prod_util.h"
#include "components/account_id/account_id.h"
#include "ui/message_center/lock_screen/lock_screen_controller.h"

namespace ash {

// This class implements the ASH logic for the message center.
class AshMessageCenterLockScreenController
    : public message_center::LockScreenController,
      public SessionObserver {
 public:
  // Returns if the message center shows the notifications on the lock screen
  // or not. True if it shows, false if doesn't.
  static ASH_EXPORT bool IsEnabled();

  // Returns if the message center on the lock screen is forcibly disabled,
  // due to the policy or the corrupt state.
  // When this returns true, |IsEnabled()| must return false.
  static ASH_EXPORT bool IsAllowed();

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

 private:
  // Modes of the lock screen notification.
  enum class Mode { PROHIBITED, HIDE, SHOW, HIDE_SENSITIVE };

  // Returns the current mode of the lock screen notification.
  static Mode GetMode();

  // Override the current mode for tests.
  // Exporting for test.
  static ASH_EXPORT void OverrideModeForTest(std::optional<Mode> new_mode);

  static std::optional<Mode> overridden_mode_for_testing_;

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
