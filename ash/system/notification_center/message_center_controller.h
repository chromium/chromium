// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_CONTROLLER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/message_center/arc_notifications_host_initializer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class PrefRegistrySimple;

namespace message_center {
class NotificationBlocker;
}

namespace ash {

class ArcNotificationManagerBase;
class AshNotificationDragController;
class FullscreenNotificationBlocker;
class PhoneHubNotificationController;
class InactiveUserNotificationBlocker;
class SessionStateNotificationBlocker;

// This class manages the ash message center and allows clients (like Chrome) to
// add and remove notifications.
class ASH_EXPORT MessageCenterController
    : public ArcNotificationsHostInitializer,
      public SessionObserver {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  MessageCenterController();

  MessageCenterController(const MessageCenterController&) = delete;
  MessageCenterController& operator=(const MessageCenterController&) = delete;

  ~MessageCenterController() override;

  // ArcNotificationsHostInitializer:
  void SetArcNotificationManagerInstance(
      std::unique_ptr<ArcNotificationManagerBase> manager_instance) override;
  ArcNotificationManagerBase* GetArcNotificationManagerInstance() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  AshNotificationDragController* drag_controller() {
    return drag_controller_.get();
  }

  InactiveUserNotificationBlocker*
  inactive_user_notification_blocker_for_testing() {
    return inactive_user_notification_blocker_.get();
  }

  SessionStateNotificationBlocker* session_state_notification_blocker() {
    return session_state_notification_blocker_.get();
  }

  PhoneHubNotificationController* phone_hub_notification_controller() {
    return phone_hub_notification_controller_.get();
  }

 private:
  std::unique_ptr<FullscreenNotificationBlocker>
      fullscreen_notification_blocker_;
  std::unique_ptr<InactiveUserNotificationBlocker>
      inactive_user_notification_blocker_;
  std::unique_ptr<SessionStateNotificationBlocker>
      session_state_notification_blocker_;
  std::unique_ptr<message_center::NotificationBlocker> all_popup_blocker_;

  std::unique_ptr<ArcNotificationManagerBase> arc_notification_manager_;

  std::unique_ptr<PhoneHubNotificationController>
      phone_hub_notification_controller_;

  // Exists only if the notification drag feature is enabled.
  std::unique_ptr<AshNotificationDragController> drag_controller_;

  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_MESSAGE_CENTER_CONTROLLER_H_
