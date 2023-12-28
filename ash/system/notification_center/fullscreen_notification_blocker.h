// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_FULLSCREEN_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_FULLSCREEN_NOTIFICATION_BLOCKER_H_

#include "ash/shell_observer.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

// A notification blocker which checks the fullscreen state.
class FullscreenNotificationBlocker
    : public message_center::NotificationBlocker,
      public ShellObserver {
 public:
  explicit FullscreenNotificationBlocker(
      message_center::MessageCenter* message_center);

  FullscreenNotificationBlocker(const FullscreenNotificationBlocker&) = delete;
  FullscreenNotificationBlocker& operator=(
      const FullscreenNotificationBlocker&) = delete;

  ~FullscreenNotificationBlocker() override;

  static bool BlockForMixedFullscreen(
      const message_center::Notification& notification,
      bool is_fullscreen);

  // message_center::NotificationBlocker:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

 private:
  // ShellObserver:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  // Set to true if all displays have a fullscreen window.
  bool all_fullscreen_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_FULLSCREEN_NOTIFICATION_BLOCKER_H_
