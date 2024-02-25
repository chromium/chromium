// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_INACTIVE_USER_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_INACTIVE_USER_NOTIFICATION_BLOCKER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "components/account_id/account_id.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

// A notification blocker for per-profile stream switching.
class ASH_EXPORT InactiveUserNotificationBlocker
    : public message_center::NotificationBlocker,
      public SessionObserver {
 public:
  explicit InactiveUserNotificationBlocker(
      message_center::MessageCenter* message_center);

  InactiveUserNotificationBlocker(const InactiveUserNotificationBlocker&) =
      delete;
  InactiveUserNotificationBlocker& operator=(
      const InactiveUserNotificationBlocker&) = delete;

  ~InactiveUserNotificationBlocker() override;

  // message_center::NotificationBlocker:
  bool ShouldShowNotification(
      const message_center::Notification& notification) const override;
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  AccountId active_account_id_;
  std::map<AccountId, bool> quiet_modes_;
  ScopedSessionObserver scoped_observer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_INACTIVE_USER_NOTIFICATION_BLOCKER_H_
