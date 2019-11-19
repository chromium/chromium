// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/arc_notifications_host_initializer.h"
#include "base/macros.h"

class PrefRegistrySimple;

namespace message_center {
class NotificationBlocker;
}

namespace ash {

class ArcNotificationManager;
class FullscreenNotificationBlocker;
class InactiveUserNotificationBlocker;
class SessionStateNotificationBlocker;

// This class manages the ash message center and allows clients (like Chrome) to
// add and remove notifications.
class ASH_EXPORT MessageCenterController
    : public ArcNotificationsHostInitializer {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  MessageCenterController();
  ~MessageCenterController() override;

  // ArcNotificationsHostInitializer:
  void SetArcNotificationsInstance(
      arc::mojom::NotificationsInstancePtr arc_notification_instance) override;

  InactiveUserNotificationBlocker*
  inactive_user_notification_blocker_for_testing() {
    return inactive_user_notification_blocker_.get();
  }

 private:
  std::unique_ptr<FullscreenNotificationBlocker>
      fullscreen_notification_blocker_;
  std::unique_ptr<InactiveUserNotificationBlocker>
      inactive_user_notification_blocker_;
  std::unique_ptr<SessionStateNotificationBlocker>
      session_state_notification_blocker_;
  std::unique_ptr<message_center::NotificationBlocker> all_popup_blocker_;

  std::unique_ptr<ArcNotificationManager> arc_notification_manager_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_CONTROLLER_H_
