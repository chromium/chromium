// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCREEN_LOCK_NOTIFICATION_BLOCKER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCREEN_LOCK_NOTIFICATION_BLOCKER_H_

#include "base/timer/timer.h"
#include "ui/message_center/notification_blocker.h"

// A notification blocker which checks the screen lock state for desktop
// platforms other than ChromeOS. The ChromeOS equivalent for this is
// LoginStateNotificationBlocker.
class ScreenLockNotificationBlocker
    : public message_center::NotificationBlocker {
 public:
  explicit ScreenLockNotificationBlocker(
      message_center::MessageCenter* message_center);
  ScreenLockNotificationBlocker(const ScreenLockNotificationBlocker&) = delete;
  ScreenLockNotificationBlocker& operator=(
      const ScreenLockNotificationBlocker&) = delete;
  ~ScreenLockNotificationBlocker() override;

  bool is_locked() const { return is_locked_; }

  // message_center::NotificationBlocker overrides:
  void CheckState() override;
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

 private:
  bool is_locked_;

  base::OneShotTimer timer_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCREEN_LOCK_NOTIFICATION_BLOCKER_H_
