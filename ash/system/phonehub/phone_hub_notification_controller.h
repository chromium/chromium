// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ash/ash_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"
#include "chromeos/ash/components/phonehub/notification_manager.h"
#include "chromeos/ash/components/phonehub/tether_controller.h"

namespace message_center {
class MessageView;
class Notification;
}  // namespace message_center

namespace ash {

namespace phonehub {
class Notification;
class NotificationInteractionHandler;
class PhoneHubManager;
class PhoneModel;

namespace proto {
class CameraRollItemMetadata;
}  // namespace proto

}  // namespace phonehub

// This controller creates and manages a message_center::Notification for each
// PhoneHub corresponding notification.
class ASH_EXPORT PhoneHubNotificationController
    : public phonehub::CameraRollManager::Observer,
      public phonehub::NotificationManager::Observer,
      public phonehub::TetherController::Observer {
 public:
  PhoneHubNotificationController();
  ~PhoneHubNotificationController() override;
  PhoneHubNotificationController(const PhoneHubNotificationController&) =
      delete;
  PhoneHubNotificationController& operator=(
      const PhoneHubNotificationController&) = delete;

  // Sets the NotifictionManager that provides the underlying PhoneHub
  // notifications.
  void SetManager(phonehub::PhoneHubManager* phone_hub_manager);

  const std::u16string GetPhoneName() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(PhoneHubNotificationControllerTest,
                           CustomActionRowExpanded);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubNotificationControllerTest,
                           ReplyBrieflyDisabled);
  FRIEND_TEST_ALL_PREFIXES(PhoneHubNotificationControllerTest,
                           NotificationHasPhoneName);

  class NotificationDelegate;

  // phonehub::NotificationManager::Observer:
  void OnNotificationsAdded(
      const base::flat_set<int64_t>& notification_ids) override;
  void OnNotificationsUpdated(
      const base::flat_set<int64_t>& notification_ids) override;
  void OnNotificationsRemoved(
      const base::flat_set<int64_t>& notification_ids) override;

  // phonehub::TetherController::Observer:
  void OnAttemptConnectionScanFailed() override;
  void OnTetherStatusChanged() override {}

  // phonehub::CameraRollManager::Observer:
  void OnCameraRollDownloadError(
      DownloadErrorType error_type,
      const phonehub::proto::CameraRollItemMetadata& metadata) override;

  // Helper functions for creating Camera Roll notifications
  std::unique_ptr<message_center::Notification>
  CreateCameraRollGenericNotification(
      const phonehub::proto::CameraRollItemMetadata& metadata);
  std::unique_ptr<message_center::Notification>
  CreateCameraRollStorageNotification(
      const phonehub::proto::CameraRollItemMetadata& metadata);
  std::unique_ptr<message_center::Notification>
  CreateCameraRollNetworkNotification(
      const phonehub::proto::CameraRollItemMetadata& metadata);

  // Callbacks for user interactions.
  void OpenSettings();
  void DismissNotification(int64_t notification_id);
  void HandleNotificationBodyClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata);
  void SendInlineReply(int64_t notification_id,
                       const std::u16string& inline_reply_text);

  // Logs the number of PhoneHub notifications.
  void LogNotificationCount();

  // Shows a Chrome OS notification for the provided phonehub::Notification.
  // If |is_update| is true, this function updates an existing notification;
  // otherwise, a new notification is created.
  void SetNotification(const phonehub::Notification* notification,
                       bool is_update);

  // Creates a message_center::Notification from the PhoneHub notification data.
  std::unique_ptr<message_center::Notification> CreateNotification(
      const phonehub::Notification* notification,
      const std::string& cros_id,
      NotificationDelegate* delegate,
      bool is_update);
  int GetSystemPriorityForNotification(
      const phonehub::Notification* notification,
      bool is_update);

  static std::unique_ptr<message_center::MessageView>
  CreateCustomNotificationView(
      base::WeakPtr<PhoneHubNotificationController> notification_controller,
      const message_center::Notification& notification,
      bool shown_in_popup);

  static std::unique_ptr<message_center::MessageView>
  CreateCustomActionNotificationView(
      base::WeakPtr<PhoneHubNotificationController> notification_controller,
      const message_center::Notification& notification,
      bool shown_in_popup);

  raw_ptr<phonehub::NotificationInteractionHandler>
      notification_interaction_handler_ = nullptr;
  raw_ptr<phonehub::NotificationManager> manager_ = nullptr;
  raw_ptr<phonehub::TetherController> tether_controller_ = nullptr;
  raw_ptr<phonehub::CameraRollManager> camera_roll_manager_ = nullptr;
  raw_ptr<phonehub::PhoneModel> phone_model_ = nullptr;
  std::unordered_map<int64_t, std::unique_ptr<NotificationDelegate>>
      notification_map_;

  base::WeakPtrFactory<PhoneHubNotificationController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_NOTIFICATION_CONTROLLER_H_
