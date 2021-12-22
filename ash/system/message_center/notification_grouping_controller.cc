// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/notification_grouping_controller.h"

#include "ash/system/message_center/ash_message_popup_collection.h"
#include "ash/system/message_center/unified_message_center_bubble.h"
#include "ash/system/message_center/unified_message_center_view.h"
#include "ash/system/message_center/unified_message_list_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class GroupedNotificationList {
 public:
  GroupedNotificationList() = default;
  GroupedNotificationList(const GroupedNotificationList& other) = delete;
  GroupedNotificationList& operator=(const GroupedNotificationList& other) =
      delete;
  ~GroupedNotificationList() = default;

  void AddGroupedNotification(const std::string& notification_id,
                              const std::string& parent_id) {
    if (notifications_in_parent_map_.find(parent_id) ==
        notifications_in_parent_map_.end()) {
      notifications_in_parent_map_[parent_id] = {};
    }

    child_parent_map_[notification_id] = parent_id;

    if (notification_id != parent_id)
      notifications_in_parent_map_[parent_id].insert(notification_id);
  }

  // Remove a single child notification from a grouped notification.
  void RemoveGroupedChildNotification(const std::string& notification_id) {
    std::string& parent_id = child_parent_map_[notification_id];
    notifications_in_parent_map_[parent_id].erase(notification_id);
    child_parent_map_.erase(notification_id);
  }

  // Clear the entire grouped notification with `parent_id`
  void ClearGroupedNotification(const std::string& parent_id) {
    notifications_in_parent_map_.erase(parent_id);
    std::vector<std::string> to_be_deleted;
    for (const auto& it : child_parent_map_) {
      if (it.second == parent_id)
        to_be_deleted.push_back(it.first);
    }
    for (const auto& child : to_be_deleted)
      child_parent_map_.erase(child);
  }

  const std::string& GetParentForChild(const std::string& child_id) {
    return child_parent_map_[child_id];
  }

  std::set<std::string>& GetGroupedNotificationsForParent(
      const std::string& parent_id) {
    return notifications_in_parent_map_[parent_id];
  }

  bool GroupedChildNotificationExists(const std::string& child_id) {
    return child_parent_map_.find(child_id) != child_parent_map_.end();
  }

  bool ParentNotificationExists(const std::string& parent_id) {
    return notifications_in_parent_map_.find(parent_id) !=
           notifications_in_parent_map_.end();
  }

  // Replaces all instances of `old_parent_id` with `new_parent_id` in
  // the `child_parent_map_`.
  void ReplaceParentId(const std::string& new_parent_id,
                       const std::string& old_parent_id) {
    // Remove entry with `new_parent_id` as a child id and replace with
    // `old_parent_id` as a child id.
    child_parent_map_.erase(new_parent_id);
    child_parent_map_[old_parent_id] = new_parent_id;

    // Replace all occurrences of `old_parent_id` with `new_parent_id`.
    std::vector<std::string> to_be_updated;
    for (const auto& child : child_parent_map_) {
      if (child.second == old_parent_id)
        to_be_updated.push_back(child.first);
    }
    for (const auto& id : to_be_updated) {
      child_parent_map_.erase(child_parent_map_.find(id));
      child_parent_map_[id] = new_parent_id;
    }
  }

 private:
  // Map for looking up the parent `notification_id` for any given notification
  // id.
  std::map<std::string, std::string> child_parent_map_;

  // Map containing a list of child notification ids per each group parent id.
  // Used to keep track of grouped notifications which already have a parent
  // notification view.
  std::map<std::string, std::set<std::string>> notifications_in_parent_map_;
};

}  // namespace

NotificationGroupingController::NotificationGroupingController(
    UnifiedSystemTray* tray)
    : tray_(tray),
      grouped_notification_list_(std::make_unique<GroupedNotificationList>()) {
  observer_.Observe(MessageCenter::Get());
}

NotificationGroupingController::~NotificationGroupingController() = default;

void NotificationGroupingController::PopulateGroupParent(
    const std::string& notification_id) {
  DCHECK(MessageCenter::Get()
             ->FindNotificationById(notification_id)
             ->group_parent());
  MessageView* parent_view =
      GetActiveNotificationViewController()->GetMessageViewForNotificationId(
          notification_id);

  // TODO(crbug/1277765) Need this check to fix crbug/1275765. However, this
  // should not be necessary if the message center bubble is initialized
  // properly. Need to monitor for empty group notifications if this check is
  // hit and fix the root cause.
  if (!parent_view) {
    LOG(WARNING) << "MessageView does not exist for notification: "
                 << notification_id;
    return;
  }

  if (!parent_view->IsManuallyExpandedOrCollapsed())
    parent_view->SetExpanded(false);

  std::vector<const Notification*> notifications;
  for (const auto* notification : MessageCenter::Get()->GetNotifications()) {
    if (notification->notifier_id() == parent_view->notifier_id() &&
        notification->id() != parent_view->notification_id()) {
      grouped_notification_list_->AddGroupedNotification(notification->id(),
                                                         notification_id);
      notifications.push_back(notification);
    }
  }
  parent_view->PopulateGroupNotifications(notifications);
}

const std::string& NotificationGroupingController::GetParentIdForChildForTest(
    const std::string& notification_id) const {
  return grouped_notification_list_->GetParentForChild(notification_id);
}
void NotificationGroupingController::SetupParentNotification(
    std::string* parent_id) {
  Notification* parent_notification =
      MessageCenter::Get()->FindNotificationById(*parent_id);
  std::unique_ptr<Notification> notification_copy =
      CreateCopyForParentNotification(*parent_notification);

  std::string new_parent_id = notification_copy->id();
  std::string old_parent_id = *parent_id;
  *parent_id = new_parent_id;

  grouped_notification_list_->AddGroupedNotification(old_parent_id,
                                                     new_parent_id);

  Notification* new_parent_notification = notification_copy.get();
  {
    // Prevent double processing `parent_notification`'s copy, used as a group
    // parent.
    base::AutoReset<bool> reset(&adding_parent_grouped_notification_, true);
    MessageCenter::Get()->AddNotification(std::move(notification_copy));
  }
  GetActiveNotificationViewController()
      ->ConvertNotificationViewToGroupedNotificationView(
          /*ungrouped_notification_id=*/old_parent_id,
          /*new_grouped_notification_id=*/new_parent_id);

  grouped_notification_list_->ReplaceParentId(
      /*new_parent_id=*/new_parent_id,
      /*old_parent_id=*/old_parent_id);

  // Add the old parent notification as a group child to the
  // newly created parent notification which will act as a
  // container for this group as long as it exists.
  new_parent_notification->SetGroupParent();
  parent_notification->SetGroupChild();

  auto* parent_view =
      GetActiveNotificationViewController()->GetMessageViewForNotificationId(
          new_parent_id);
  if (parent_view) {
    parent_view->UpdateWithNotification(*new_parent_notification);
    // Grouped notifications should start off in the collapsed state.
    parent_view->SetExpanded(false);
    parent_view->AddGroupNotification(*parent_notification,
                                      /*newest_first=*/false);
  }
}

void NotificationGroupingController::
    SetupSingleNotificationFromGroupedNotification(
        const std::string& group_parent_id,
        const std::string& new_single_notification_id) {
  auto* message_center = MessageCenter::Get();
  MessageView* parent_view =
      GetActiveNotificationViewController()->GetMessageViewForNotificationId(
          group_parent_id);
  auto* new_single_notification =
      message_center->FindNotificationById(new_single_notification_id);

  // These could already have been removed in case of a clear all action.
  // Therefore, do not do anything if either of them has already been removed.
  if (!parent_view || !new_single_notification)
    return;

  parent_view->RemoveGroupNotification(new_single_notification_id);
  parent_view->UpdateWithNotification(*new_single_notification);

  GetActiveNotificationViewController()
      ->ConvertGroupedNotificationViewToNotificationView(
          /*grouped_notification_id=*/group_parent_id,
          /*new_single_notification_id=*/new_single_notification_id);

  message_center->FindNotificationById(group_parent_id)->ClearGroupParent();
  new_single_notification->ClearGroupChild();

  grouped_notification_list_->ClearGroupedNotification(group_parent_id);

  message_center->RemoveNotification(group_parent_id, /*by_user=*/false);
}

std::unique_ptr<Notification>
NotificationGroupingController::CreateCopyForParentNotification(
    const Notification& parent_notification) {
  // Create a copy with a timestamp that is older than the copied notification.
  // We need to set an older timestamp so that this notification will become
  // the parent notification for it's notifier_id.
  auto child_copy = std::make_unique<Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      parent_notification.id() +
          message_center::kIdSuffixForGroupContainerNotification,
      parent_notification.title(), parent_notification.message(), gfx::Image(),
      std::u16string(), parent_notification.origin_url(),
      parent_notification.notifier_id(), message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  child_copy->set_timestamp(parent_notification.timestamp() -
                            base::Milliseconds(1));
  child_copy->SetGroupChild();

  return child_copy;
}

void NotificationGroupingController::RemoveGroupedChild(
    const std::string& notification_id) {
  if (!grouped_notification_list_->GroupedChildNotificationExists(
          notification_id)) {
    return;
  }

  const std::string parent_id =
      grouped_notification_list_->GetParentForChild(notification_id);

  MessageView* parent_view =
      GetActiveNotificationViewController()->GetMessageViewForNotificationId(
          parent_id);
  if (parent_view)
    parent_view->RemoveGroupNotification(notification_id);

  grouped_notification_list_->RemoveGroupedChildNotification(notification_id);

  // Convert back to a single notification if there is only one
  // group child left in the group notification.
  auto grouped_notifications =
      grouped_notification_list_->GetGroupedNotificationsForParent(parent_id);
  if (GetActiveNotificationViewController()->GetMessageViewForNotificationId(
          parent_id) &&
      grouped_notifications.size() == 1) {
    SetupSingleNotificationFromGroupedNotification(
        /*group_parent_id=*/parent_id,
        /*new_single_notification_id=*/*grouped_notifications.begin());
  }
}

message_center::NotificationViewController*
NotificationGroupingController::GetActiveNotificationViewController() {
  if (tray_->IsMessageCenterBubbleShown()) {
    return tray_->message_center_bubble()
        ->message_center_view()
        ->message_list_view();
  } else {
    return tray_->GetMessagePopupCollection();
  }
}

void NotificationGroupingController::OnNotificationAdded(
    const std::string& notification_id) {
  // Do not double process a notification that was re-added as a grouped parent.
  if (adding_parent_grouped_notification_)
    return;

  auto* message_center = MessageCenter::Get();
  Notification* notification =
      message_center->FindNotificationById(notification_id);

  // We only need to process notifications that are children of an
  // existing group. So do nothing otherwise.
  if (!notification)
    return;

  if (!notification->group_child())
    return;

  Notification* parent_notification =
      message_center->FindParentNotificationForOriginUrl(
          notification->origin_url());
  std::string parent_id = parent_notification->id();

  // If we are creating a new notification group for this `notifier_id`,
  // we must create a copy of the designated parent notification and
  // use it to set up a container notification which will hold all
  // notifications for this group.
  if (!grouped_notification_list_->ParentNotificationExists(parent_id))
    SetupParentNotification(&parent_id);

  grouped_notification_list_->AddGroupedNotification(notification_id,
                                                     parent_id);

  MessageView* parent_view =
      GetActiveNotificationViewController()->GetMessageViewForNotificationId(
          parent_id);
  if (parent_view)
    parent_view->AddGroupNotification(*notification, /*newest_first=*/false);
  else
    message_center->ResetSinglePopup(parent_id);
}

void NotificationGroupingController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  if (grouped_notification_list_->ParentNotificationExists(notification_id))
    PopulateGroupParent(notification_id);
}

void NotificationGroupingController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (grouped_notification_list_->GroupedChildNotificationExists(
          notification_id)) {
    RemoveGroupedChild(notification_id);
  }

  if (grouped_notification_list_->ParentNotificationExists(notification_id)) {
    std::vector<std::string> to_be_deleted;
    auto grouped_notifications =
        grouped_notification_list_->GetGroupedNotificationsForParent(
            notification_id);
    std::copy(grouped_notifications.begin(), grouped_notifications.end(),
              std::back_inserter(to_be_deleted));
    grouped_notification_list_->ClearGroupedNotification(notification_id);

    for (const auto& id : to_be_deleted)
      MessageCenter::Get()->RemoveNotification(id, by_user);
  }
}

}  // namespace ash
