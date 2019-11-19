// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/fullscreen_notification_blocker.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

FullscreenNotificationBlocker::FullscreenNotificationBlocker(
    message_center::MessageCenter* message_center)
    : NotificationBlocker(message_center) {
  Shell::Get()->AddShellObserver(this);
}

FullscreenNotificationBlocker::~FullscreenNotificationBlocker() {
  Shell::Get()->RemoveShellObserver(this);
}

bool FullscreenNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  // Show the notification if any of the following are true:
  // - we're not in fullscreen
  // - the notification explicitly asked to be shown over fullscreen
  // - the notification's priority is SYSTEM_PRIORITY
  bool enabled = !should_block_ ||
                 (notification.fullscreen_visibility() !=
                  message_center::FullscreenVisibility::NONE) ||
                 notification.priority() == message_center::SYSTEM_PRIORITY;

  if (enabled && !should_block_) {
    UMA_HISTOGRAM_ENUMERATION("Notifications.Display_Windowed",
                              notification.notifier_id().type);
  }

  return enabled;
}

void FullscreenNotificationBlocker::OnFullscreenStateChanged(
    bool is_fullscreen,
    aura::Window* container) {
  aura::Window* root_window = container->GetRootWindow();
  if (root_window != Shell::GetRootWindowForNewWindows())
    return;

  RootWindowController* controller =
      RootWindowController::ForWindow(root_window);

  // During shutdown |controller| can be NULL.
  if (!controller)
    return;

  // Block notifications if the shelf is hidden because of a fullscreen
  // window.
  const aura::Window* fullscreen_window =
      controller->GetWindowForFullscreenMode();
  bool was_blocked = should_block_;
  should_block_ =
      fullscreen_window &&
      WindowState::Get(fullscreen_window)->GetHideShelfWhenFullscreen();
  if (was_blocked != should_block_)
    NotifyBlockingStateChanged();
}

}  // namespace ash
