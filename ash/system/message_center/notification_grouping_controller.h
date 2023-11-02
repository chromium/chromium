// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_GROUPING_CONTROLLER_H_
#define ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_GROUPING_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class NotificationViewController;
}  // namespace message_center

namespace ash {

namespace {
class GroupedNotificationList;
}  // namespace

class UnifiedSystemTray;

// A controller class to manage adding, removing and updating group
// notifications.
class NotificationGroupingController
    : public message_center::MessageCenterObserver {
 public:
  explicit NotificationGroupingController(UnifiedSystemTray* tray);
  NotificationGroupingController(const NotificationGroupingController& other) =
      delete;
  NotificationGroupingController& operator=(
      const NotificationGroupingController& other) = delete;
  ~NotificationGroupingController() override;

  // MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  void ConvertFromSingleToGroupNotificationAfterAnimation(
      const std::string& notification_id,
      std::string& parent_id,
      message_center::Notification* parent_notification);

  message_center::NotificationViewController*
  GetActiveNotificationViewController();

 protected:
  // Adds grouped child notifications that belong to a parent message
  // view.
  void PopulateGroupParent(const std::string& notification_id);

  const std::string& GetParentIdForChildForTest(
      const std::string& notification_id) const;

 private:
  friend class NotificationGroupingControllerTest;

  // Sets up a parent view to hold all message views for
  // a grouped notification. Does this by creating a copy of the
  // parent notification and switching the notification_ids of the
  // current message view associated with the parent notification.
  // Returns the new parent_id for the newly created  copy.
  const std::string& SetupParentNotification(
      message_center::Notification* parent_notification,
      const std::string& parent_id);

  // Creates a copy notification that will act as a parent notification
  // for its group.
  std::unique_ptr<message_center::Notification> CreateCopyForParentNotification(
      const message_center::Notification& parent_notification);

  // Remove `notification_id` from `child_parent_map` and
  // `notifications_in_parent_map` Also remove from it's parent notification's
  // view if if the view currently exists.
  void RemoveGroupedChild(const std::string& notification_id);

  // Whether a grouped parent notification is being added to MessageCenter. Used
  // to prevent an infinite loop.
  bool adding_parent_grouped_notification_ = false;

  UnifiedSystemTray* const tray_;

  // A data structure that holds all grouped notifications along with their
  // associations with their parent notifications. This pointer is assigned to a
  // static global instance that is shared across all instances of
  // `NotificationGroupingController`.
  GroupedNotificationList* const grouped_notification_list_;

  base::ScopedObservation<message_center::MessageCenter, MessageCenterObserver>
      observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_NOTIFICATION_GROUPING_CONTROLLER_H_
