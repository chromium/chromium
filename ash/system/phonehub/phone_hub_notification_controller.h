// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "chromeos/components/phonehub/notification_manager.h"

namespace chromeos {
namespace phonehub {
class Notification;
}  // namespace phonehub
}  // namespace chromeos

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// This controller creates and manages a message_center::Notification for each
// PhoneHub corresponding notification.
class ASH_EXPORT PhoneHubNotificationController
    : public chromeos::phonehub::NotificationManager::Observer {
 public:
  PhoneHubNotificationController();
  ~PhoneHubNotificationController() override;
  PhoneHubNotificationController(const PhoneHubNotificationController&) =
      delete;
  PhoneHubNotificationController& operator=(
      const PhoneHubNotificationController&) = delete;

  // Sets the NotifictionManager that provides the underlying PhoneHub
  // notifications.
  void SetManager(chromeos::phonehub::NotificationManager* manager);

 private:
  class NotificationDelegate;

  // chromeos::phonehub::NotificationManager::Observer:
  void OnNotificationsAdded(
      const base::flat_set<int64_t>& notification_ids) override;
  void OnNotificationsUpdated(
      const base::flat_set<int64_t>& notification_ids) override;
  void OnNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids) override;

  // Callbacks for user interactions.
  void OpenSettings();
  void DismissNotification(int64_t notification_id);
  void SendInlineReply(int64_t notification_id,
                       const base::string16& inline_reply_text);

  // Creates or updates a ChromeOS notification for the given PhoneHub
  // notification data.
  void CreateOrUpdateNotification(
      const chromeos::phonehub::Notification* notification);

  // Creates a message_center::Notification from the PhoneHub notification data.
  std::unique_ptr<message_center::Notification> CreateNotification(
      const chromeos::phonehub::Notification* notification,
      const std::string& cros_id,
      NotificationDelegate* delegate);

  chromeos::phonehub::NotificationManager* manager_ = nullptr;
  std::unordered_map<int64_t, std::unique_ptr<NotificationDelegate>>
      notification_map_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_
