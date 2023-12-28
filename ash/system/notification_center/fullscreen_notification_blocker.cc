// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/fullscreen_notification_blocker.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {
namespace {

bool ShouldShow(const message_center::Notification& notification,
                bool is_fullscreen,
                bool include_uma) {
  // Show the notification if any of the following are true:
  // - we're not in fullscreen
  // - the notification explicitly asked to be shown over fullscreen
  // - the notification's priority is SYSTEM_PRIORITY
  bool enabled = !is_fullscreen ||
                 (notification.fullscreen_visibility() !=
                  message_center::FullscreenVisibility::NONE) ||
                 notification.priority() == message_center::SYSTEM_PRIORITY;

  if (include_uma && enabled && !is_fullscreen) {
    UMA_HISTOGRAM_ENUMERATION("Notifications.Display_Windowed",
                              notification.notifier_id().type);
  }

  return enabled;
}

}  // namespace

// static
bool FullscreenNotificationBlocker::BlockForMixedFullscreen(
    const message_center::Notification& notification,
    bool is_fullscreen) {
  return !ShouldShow(notification, is_fullscreen,
                     /*include_uma=*/false);
}

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
  return ShouldShow(notification, all_fullscreen_, /*include_uma=*/true);
}

void FullscreenNotificationBlocker::OnFullscreenStateChanged(
    bool is_fullscreen,
    aura::Window* container) {
  // Block notifications if all displays have a fullscreen window.  Otherwise
  // include the notification and only fullscreen windows will filter it.
  all_fullscreen_ = true;
  for (auto* controller : RootWindowController::root_window_controllers()) {
    // During shutdown |controller| can be nullptr.
    controller = RootWindowController::ForWindow(controller->GetRootWindow());

    if (controller && !controller->IsInFullscreenMode()) {
      all_fullscreen_ = false;
      break;
    }
  }

  // Any change to fullscreen state on any of the displays requires
  // MessagePopupCollection instances to recheck.
  NotifyBlockingStateChanged();
}

}  // namespace ash
