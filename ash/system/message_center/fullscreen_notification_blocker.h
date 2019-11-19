// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_FULLSCREEN_NOTIFICATION_BLOCKER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_FULLSCREEN_NOTIFICATION_BLOCKER_H_

#include "ash/shell_observer.h"
#include "base/macros.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

// A notification blocker which checks the fullscreen state.
class FullscreenNotificationBlocker
    : public message_center::NotificationBlocker,
      public ShellObserver {
 public:
  explicit FullscreenNotificationBlocker(
      message_center::MessageCenter* message_center);
  ~FullscreenNotificationBlocker() override;

  // message_center::NotificationBlocker:
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override;

 private:
  // ShellObserver:
  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override;

  bool should_block_ = false;

  DISALLOW_COPY_AND_ASSIGN(FullscreenNotificationBlocker);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_FULLSCREEN_NOTIFICATION_BLOCKER_H_
