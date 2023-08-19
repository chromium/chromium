// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/fullscreen_notification_blocker.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/fullscreen.h"

namespace {
const int kFullscreenStatePollingIntervalSeconds = 1;
}

FullscreenNotificationBlocker::FullscreenNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center), is_fullscreen_mode_(false) {}

FullscreenNotificationBlocker::~FullscreenNotificationBlocker() {
}

void FullscreenNotificationBlocker::CheckState() {
  bool was_fullscreen_mode = is_fullscreen_mode_;
  is_fullscreen_mode_ = IsFullScreenMode();
  if (is_fullscreen_mode_ != was_fullscreen_mode)
    NotifyBlockingStateChanged();

  if (is_fullscreen_mode_) {
    timer_.Start(FROM_HERE,
                 base::Seconds(kFullscreenStatePollingIntervalSeconds), this,
                 &FullscreenNotificationBlocker::CheckState);
  }
}

bool FullscreenNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  bool enabled =
      !is_fullscreen_mode_ || (notification.fullscreen_visibility() !=
                               message_center::FullscreenVisibility::NONE);

  if (enabled && !is_fullscreen_mode_) {
    UMA_HISTOGRAM_ENUMERATION("Notifications.Display_Windowed",
                              notification.notifier_id().type);
  }

  return enabled;
}
