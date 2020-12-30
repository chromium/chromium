// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "chromeos/components/phonehub/feature_status_provider.h"
#include "chromeos/components/phonehub/notification_manager.h"
#include "chromeos/components/phonehub/tether_controller.h"

namespace chromeos {
namespace phonehub {
class Notification;
class PhoneHubManager;
class PhoneModel;
}  // namespace phonehub
}  // namespace chromeos

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

// This controller creates and manages a message_center::Notification for each
// PhoneHub corresponding notification.
class ASH_EXPORT PhoneHubNotificationController
    : public chromeos::phonehub::FeatureStatusProvider::Observer,
      public chromeos::phonehub::NotificationManager::Observer,
      public chromeos::phonehub::TetherController::Observer {
 public:
  PhoneHubNotificationController();
  ~PhoneHubNotificationController() override;
  PhoneHubNotificationController(const PhoneHubNotificationController&) =
      delete;
  PhoneHubNotificationController& operator=(
      const PhoneHubNotificationController&) = delete;

  // Sets the NotifictionManager that provides the underlying PhoneHub
  // notifications.
  void SetManager(chromeos::phonehub::PhoneHubManager* phone_hub_manager);

  const base::string16 GetPhoneName() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneHubNotificationControllerTest,
                           ReplyBrieflyDisabled);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubNotificationControllerTest,
                           NotificationHasPhoneName);

  class NotificationDelegate;

  // chromeos::phonehub::FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

  // chromeos::phonehub::NotificationManager::Observer:
  void OnNotificationsAdded(
      const base::flat_set<int64_t>& notification_ids) override;
  void OnNotificationsUpdated(
      const base::flat_set<int64_t>& notification_ids) override;
  void OnNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids) override;

  // chromeos::phonehub::TetherController::Observer:
  void OnAttemptConnectionScanFailed() override;
  void OnTetherStatusChanged() override {}

  // Callbacks for user interactions.
  void OpenSettings();
  void DismissNotification(int64_t notification_id);
  void SendInlineReply(int64_t notification_id,
                       const base::string16& inline_reply_text);

  // Logs the number of PhoneHub notifications.
  void LogNotificationCount();

  // Shows a Chrome OS notification for the provided phonehub::Notification.
  // If |is_update| is true, this function updates an existing notification;
  // otherwise, a new notification is created.
  void SetNotification(const chromeos::phonehub::Notification* notification,
                       bool is_update);

  // Creates a message_center::Notification from the PhoneHub notification data.
  std::unique_ptr<message_center::Notification> CreateNotification(
      const chromeos::phonehub::Notification* notification,
      const std::string& cros_id,
      NotificationDelegate* delegate,
      bool is_update);
  int GetSystemPriorityForNotification(
      const chromeos::phonehub::Notification* notification,
      bool is_update);

  static std::unique_ptr<message_center::MessageView>
  CreateCustomNotificationView(
      base::WeakPtr<PhoneHubNotificationController> notification_controller,
      const message_center::Notification& notification);

  chromeos::phonehub::NotificationManager* manager_ = nullptr;
  chromeos::phonehub::FeatureStatusProvider* feature_status_provider_ = nullptr;
  chromeos::phonehub::TetherController* tether_controller_ = nullptr;
  chromeos::phonehub::PhoneModel* phone_model_ = nullptr;
  std::unordered_map<int64_t, std::unique_ptr<NotificationDelegate>>
      notification_map_;

  // A set of notification ids that have been previously shown, even across
  // disconnects. This is needed to prevent pop-ups from reappearing due to a
  // flaky connection. See crbug.com/1150621.
  std::unordered_set<uint64_t> shown_notification_ids_;

  base::WeakPtrFactory<PhoneHubNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_
