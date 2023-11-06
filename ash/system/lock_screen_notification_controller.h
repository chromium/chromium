// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_LOCK_SCREEN_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_LOCK_SCREEN_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/scoped_observation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// Controller class to manage the "Lock screen" notification.
class ASH_EXPORT LockScreenNotificationController
    : public message_center::MessageCenterObserver,
      public SessionObserver {
 public:
  LockScreenNotificationController();

  LockScreenNotificationController(const LockScreenNotificationController&) =
      delete;
  LockScreenNotificationController& operator=(
      const LockScreenNotificationController&) = delete;

  ~LockScreenNotificationController() override;

  static const char kLockScreenNotificationId[];

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SessionStateNotificationBlockerTest,
                           LockScreenNotification);

  std::unique_ptr<message_center::Notification> CreateNotification();

  bool is_screen_locked_ = false;

  base::ScopedObservation<message_center::MessageCenter,
                          message_center::MessageCenterObserver>
      message_center_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_LOCK_SCREEN_NOTIFICATION_CONTROLLER_H_
