// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/screen_lock_notification_blocker.h"

#include "base/time/time.h"
#include "ui/base/idle/idle.h"

namespace {
const int kUserStatePollingIntervalSeconds = 1;
}

ScreenLockNotificationBlocker::ScreenLockNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center), is_locked_(false) {}

ScreenLockNotificationBlocker::~ScreenLockNotificationBlocker() {
}

void ScreenLockNotificationBlocker::CheckState() {
  bool was_locked = is_locked_;
  is_locked_ = ui::CheckIdleStateIsLocked();
  if (is_locked_ != was_locked)
    NotifyBlockingStateChanged();

  if (is_locked_) {
    timer_.Start(FROM_HERE, base::Seconds(kUserStatePollingIntervalSeconds),
                 this, &ScreenLockNotificationBlocker::CheckState);
  }
}

bool ScreenLockNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  return !is_locked_;
}
