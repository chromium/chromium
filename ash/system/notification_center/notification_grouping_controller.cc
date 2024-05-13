// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_grouping_controller.h"

#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/views/ash_notification_view.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/notification_center/views/notification_list_view.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_view_controller.h"
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
    if (!base::Contains(notifications_in_parent_map_, parent_id)) {
      notifications_in_parent_map_[parent_id] = {};
    }

    child_parent_map_[notification_id] = parent_id;

    if (notification_id != parent_id) {
      notifications_in_parent_map_[parent_id].insert(notification_id);
    }
  }

  // Remove a single child notification from a grouped notification.
  void RemoveGroupedChildNotification(const std::string& notification_id) {
    DCHECK(base::Contains(child_parent_map_, notification_id));
    const std::string& parent_id = child_parent_map_[notification_id];
    notifications_in_parent_map_[parent_id].erase(notification_id);
    child_parent_map_.erase(notification_id);
  }

  // Clear the entire grouped notification with `parent_id`
  void ClearGroupedNotification(const std::string& parent_id) {
    notifications_in_parent_map_.erase(parent_id);
    std::vector<std::string> to_be_deleted;
    for (const auto& it : child_parent_map_) {
      if (it.second == parent_id) {
        to_be_deleted.push_back(it.first);
      }
    }
    for (const auto& child : to_be_deleted) {
      child_parent_map_.erase(child);
    }
  }

  const std::string& GetParentForChild(const std::string& child_id) {
    return child_parent_map_[child_id];
  }

  std::set<std::string>& GetGroupedNotificationsForParent(
      const std::string& parent_id) {
    return notifications_in_parent_map_[parent_id];
  }

  bool GroupedChildNotificationExists(const std::string& child_id) {
    return base::Contains(child_parent_map_, child_id);
  }

  bool ParentNotificationExists(const std::string& parent_id) {
    return base::Contains(notifications_in_parent_map_, parent_id);
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
      if (child.second == old_parent_id) {
        to_be_updated.push_back(child.first);
      }
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

// Needs to be a static instance because we need a single instance to be shared
// across multiple instances of `NotificationGroupingController`. When there are
// multiple screens, each screen has it's own `MessagePopupCollection`,
// `UnifiedSystemTray`, `NotificationGroupingController` etc.
GroupedNotificationList& GetGroupedNotificationListInstance() {
  static base::NoDestructor<GroupedNotificationList> instance;
  return *instance;
}

}  // namespace

NotificationGroupingController::NotificationGroupingController(
    NotificationCenterTray* notification_tray)
    : notification_tray_(notification_tray),
      grouped_notification_list_(&GetGroupedNotificationListInstance()) {
  observer_.Observe(MessageCenter::Get());
}

NotificationGroupingController::~NotificationGroupingController() = default;

void NotificationGroupingController::PopulateGroupParent(
    const std::string& notification_id) {
  DCHECK(MessageCenter::Get()
             ->FindNotificationById(notification_id)
             ->group_parent());
  MessageView* parent_view =
      GetActiveNotificationViewController()
          ? GetActiveNotificationViewController()
                ->GetMessageViewForNotificationId(notification_id)
          : nullptr;

  // TODO(crbug.com/40809802) Need this check to fix crbug/1275765. However,
  // this should not be necessary if the message center bubble is initialized
  // properly. Need to monitor for empty group notifications if this check is
  // hit and fix the root cause.
  if (!parent_view) {
    LOG(WARNING) << "MessageView does not exist for notification: "
                 << notification_id;
    return;
  }

  if (!parent_view->IsManuallyExpandedOrCollapsed()) {
    parent_view->SetExpanded(false);
  }

  std::vector<const Notification*> notifications;
  for (Notification* notification : MessageCenter::Get()->GetNotifications()) {
    if (notification->notifier_id() == parent_view->notifier_id() &&
        notification->id() != parent_view->notification_id()) {
      notification->SetGroupChild();
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

void NotificationGroupingController::
    ConvertFromSingleToGroupNotificationAfterAnimation(
        const std::string& notification_id,
        std::string& parent_id,
        Notification* parent_notification) {
  auto* message_center = MessageCenter::Get();

  auto* notification_view_controller = GetActiveNotificationViewController();
  if (!notification_view_controller) {
    return;
  }

  Notification* notification =
      message_center->FindNotificationById(notification_id);
  if (!notification) {
    return;
  }

  parent_id = SetupParentNotification(parent_notification, parent_id);

  AddNotificationToGroup(notification_id, parent_id);
}

const std::string& NotificationGroupingController::SetupParentNotification(
    Notification* parent_notification,
    const std::string& parent_id) {
  std::unique_ptr<Notification> notification_copy =
      CreateCopyForParentNotification(*parent_notification);

  std::string new_parent_id = notification_copy->id();
  std::string old_parent_id = parent_id;

  grouped_notification_list_->AddGroupedNotification(old_parent_id,
                                                     new_parent_id);

  // Since MessagePopupCollection uses the global MessageCenter to update its
  // animations, the update to MessageCenter in primary display will interfere
  // the animation in the secondary display. Thus, calling this functions on all
  // display is needed.
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    auto* controller =
        message_center_utils::GetActiveNotificationViewControllerForDisplay(
            display.id());
    if (!controller) {
      continue;
    }
    controller->ConvertNotificationViewToGroupedNotificationView(
        /*ungrouped_notification_id=*/old_parent_id,
        /*new_grouped_notification_id=*/new_parent_id);
  }

  grouped_notification_list_->ReplaceParentId(
      /*new_parent_id=*/new_parent_id,
      /*old_parent_id=*/old_parent_id);

  // Add the old parent notification as a group child to the
  // newly created parent notification which will act as a
  // container for this group as long as it exists.
  notification_copy->SetGroupParent();
  parent_notification->SetGroupChild();

  Notification* new_parent_notification = notification_copy.get();
  {
    // Prevent double processing `parent_notification`'s copy, used as a group
    // parent.
    base::AutoReset<bool> reset(&adding_parent_grouped_notification_, true);
    MessageCenter::Get()->AddNotification(std::move(notification_copy));
  }

  // Record metrics for the new parent and new child added.
  metrics_utils::LogGroupNotificationAddedType(
      metrics_utils::GroupNotificationType::GROUP_PARENT);
  metrics_utils::LogGroupNotificationAddedType(
      metrics_utils::GroupNotificationType::GROUP_CHILD);

  auto* parent_view = GetActiveNotificationViewController()
                          ? GetActiveNotificationViewController()
                                ->GetMessageViewForNotificationId(new_parent_id)
                          : nullptr;
  if (parent_view) {
    parent_view->UpdateWithNotification(*new_parent_notification);
    // Grouped notifications should start off in the collapsed state.
    parent_view->SetExpanded(false);
    parent_view->AddGroupNotification(*parent_notification);
  }

  return new_parent_notification->id();
}

std::unique_ptr<Notification>
NotificationGroupingController::CreateCopyForParentNotification(
    const Notification& parent_notification) {
  // Create a copy with a timestamp that is older than the copied notification.
  // We need to set an older timestamp so that this notification will become
  // the parent notification for it's notifier_id.
  auto copy = std::make_unique<Notification>(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      parent_notification.id() +
          message_center_utils::GenerateGroupParentNotificationIdSuffix(
              parent_notification.notifier_id()),
      parent_notification.title(), parent_notification.message(),
      ui::ImageModel(), parent_notification.display_source(),
      parent_notification.origin_url(), parent_notification.notifier_id(),
      message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  copy->set_timestamp(parent_notification.timestamp() - base::Milliseconds(1));
  copy->set_settings_button_handler(
      parent_notification.rich_notification_data().settings_button_handler);
  copy->set_fullscreen_visibility(parent_notification.fullscreen_visibility());
  if (parent_notification.delegate()) {
    copy->set_delegate(
        parent_notification.delegate()->GetDelegateForParentCopy());
  }
  copy->set_vector_small_image(parent_notification.parent_vector_small_image());
  copy->SetSmallImage(parent_notification.small_image());

  if (parent_notification.accent_color_id().has_value()) {
    copy->set_accent_color_id(parent_notification.accent_color_id().value());
  }

  if (parent_notification.accent_color().has_value()) {
    copy->set_accent_color(parent_notification.accent_color().value());
  }

  copy->set_priority(parent_notification.priority());
  copy->set_pinned(parent_notification.pinned());

  // After copying, set to be a group parent.
  copy->SetGroupParent();

  return copy;
}

void NotificationGroupingController::RemoveGroupedChild(
    const std::string& notification_id) {
  if (!grouped_notification_list_->GroupedChildNotificationExists(
          notification_id)) {
    return;
  }

  const std::string parent_id =
      grouped_notification_list_->GetParentForChild(notification_id);

  // Remove parent notification if we are removing the last child notification
  // in a grouped notification.
  auto grouped_notifications =
      grouped_notification_list_->GetGroupedNotificationsForParent(parent_id);
  if (grouped_notifications.size() == 1) {
    MessageCenter::Get()->RemoveNotification(parent_id, /*by_user=*/false);
    return;
  }

  grouped_notification_list_->RemoveGroupedChildNotification(notification_id);
}

message_center::NotificationViewController*
NotificationGroupingController::GetActiveNotificationViewController() {
  if (message_center::MessageCenter::Get()->IsMessageCenterVisible()) {
    return notification_tray_->GetNotificationListView();
  }
  return notification_tray_->popup_collection();
}

void NotificationGroupingController::OnNotificationAdded(
    const std::string& notification_id) {
  // Do not double process a notification that was re-added as a grouped parent.
  if (adding_parent_grouped_notification_) {
    return;
  }

  auto* message_center = MessageCenter::Get();
  Notification* notification =
      message_center->FindNotificationById(notification_id);

  // If we are adding a notification that starts as a parent,
  // make sure to add it into the grouped notification list.
  if (notification && notification->group_parent() &&
      !grouped_notification_list_->ParentNotificationExists(notification_id)) {
    grouped_notification_list_->AddGroupedNotification(notification_id,
                                                       notification_id);
  }

  // We only need to process notifications that are children of an
  // existing group. So do nothing otherwise.
  if (!notification || !notification->group_child()) {
    return;
  }

  Notification* parent_notification =
      message_center->FindParentNotification(notification);
  if (!parent_notification) {
    return;
  }
  std::string parent_id = parent_notification->id();

  auto* parent_view = (GetActiveNotificationViewController())
                          ? GetActiveNotificationViewController()
                                ->GetMessageViewForNotificationId(parent_id)
                          : nullptr;

  // If we are creating a new notification group for this `notifier_id`,
  // we must create a copy of the designated parent notification and
  // use it to set up a container notification which will hold all
  // notifications for this group.
  if (!grouped_notification_list_->ParentNotificationExists(parent_id)) {
    if (parent_view) {
      // When there's a parent view that exists in the UI, we will perform the
      // "convert from single notification to group notification" animation.
      // Thus, the rest of the logic of adding a child notification to a group
      // will be handled in
      // `ConvertFromSingleToGroupNotificationAfterAnimation()`, which is called
      // after the animation from the parent view is completed.
      parent_view->AnimateSingleToGroup(notification_id, parent_id);
      return;
    }

    parent_id = SetupParentNotification(
        MessageCenter::Get()->FindNotificationById(parent_id), parent_id);
  }

  AddNotificationToGroup(notification_id, parent_id);
}

void NotificationGroupingController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  if (grouped_notification_list_->ParentNotificationExists(notification_id)) {
    PopulateGroupParent(notification_id);
  }
}

void NotificationGroupingController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  auto* message_center = MessageCenter::Get();
  if (grouped_notification_list_->GroupedChildNotificationExists(
          notification_id)) {
    const std::string parent_id =
        grouped_notification_list_->GetParentForChild(notification_id);

    RemoveGroupedChild(notification_id);

    if (message_center->FindPopupNotificationById(parent_id)) {
      message_center->ResetPopupTimer(parent_id);
    }

    auto* parent_notification = message_center->FindNotificationById(parent_id);
    if (parent_notification && parent_notification->pinned()) {
      // Updates to make sure if we should un-pin this parent notification.
      UpdateParentNotificationPinnedState(parent_id);
    }

    metrics_utils::LogCountOfNotificationsInOneGroup(
        grouped_notification_list_->GetGroupedNotificationsForParent(parent_id)
            .size());
  }

  if (grouped_notification_list_->ParentNotificationExists(notification_id)) {
    std::vector<std::string> to_be_deleted;
    auto grouped_notifications =
        grouped_notification_list_->GetGroupedNotificationsForParent(
            notification_id);
    base::ranges::copy(grouped_notifications,
                       std::back_inserter(to_be_deleted));
    grouped_notification_list_->ClearGroupedNotification(notification_id);

    for (const auto& id : to_be_deleted) {
      message_center->RemoveNotification(id, by_user);
    }
  }
}

void NotificationGroupingController::OnNotificationUpdated(
    const std::string& notification_id) {
  auto* message_center = MessageCenter::Get();
  Notification* notification =
      message_center->FindNotificationById(notification_id);

  if (!notification) {
    return;
  }
  ReparentNotificationIfNecessary(notification);

  // If we are updating a notification that starts as a parent,
  // make sure to add it into the grouped notification list.
  if (notification->group_parent() &&
      !grouped_notification_list_->ParentNotificationExists(notification_id)) {
    grouped_notification_list_->AddGroupedNotification(notification_id,
                                                       notification_id);
  }
  if (!notification->group_child()) {
    return;
  }

  const std::string parent_id =
      grouped_notification_list_->GetParentForChild(notification_id);
  Notification* parent_notification =
      MessageCenter::Get()->FindNotificationById(parent_id);

  auto* notification_view_controller = GetActiveNotificationViewController();
  if (notification_view_controller) {
    notification_view_controller->OnChildNotificationViewUpdated(
        parent_id, notification_id);
  }

  if (parent_notification && notification->pinned()) {
    parent_notification->set_pinned(true);
    return;
  }

  if (parent_notification && parent_notification->pinned()) {
    // Updates to make sure if we should un-pin this parent notification.
    UpdateParentNotificationPinnedState(parent_id);
  }
}

void NotificationGroupingController::AddNotificationToGroup(
    const std::string& notification_id,
    const std::string& parent_id) {
  auto* message_center = MessageCenter::Get();
  Notification* notification =
      message_center->FindNotificationById(notification_id);
  Notification* parent_notification =
      MessageCenter::Get()->FindNotificationById(parent_id);

  if (!notification || !parent_notification) {
    return;
  }

  // The parent notification should have the same priority as the last added
  // child notification, so that we can display the group popup according to
  // that notification.
  parent_notification->set_priority(notification->priority());

  // The parent notification should be pin if any of its children notifications
  // are pinned.
  if (notification->pinned()) {
    parent_notification->set_pinned(true);
  }

  grouped_notification_list_->AddGroupedNotification(notification_id,
                                                     parent_id);

  auto* notification_view_controller = GetActiveNotificationViewController();
  auto* parent_view =
      notification_view_controller
          ? notification_view_controller->GetMessageViewForNotificationId(
                parent_id)
          : nullptr;
  if (parent_view) {
    parent_view->AddGroupNotification(*notification);
    if (message_center->FindPopupNotificationById(parent_id)) {
      message_center->ResetPopupTimer(parent_id);
    }
  } else {
    message_center->ResetSinglePopup(parent_id);
  }

  metrics_utils::LogCountOfNotificationsInOneGroup(
      grouped_notification_list_->GetGroupedNotificationsForParent(parent_id)
          .size());
  metrics_utils::LogGroupNotificationAddedType(
      metrics_utils::GroupNotificationType::GROUP_CHILD);
}

void NotificationGroupingController::UpdateParentNotificationPinnedState(
    const std::string& parent_id) {
  auto* message_center = MessageCenter::Get();
  auto* parent_notification = message_center->FindNotificationById(parent_id);
  if (!parent_notification) {
    return;
  }

  // The parent notification should be pinned if at least one of the child is
  // pinned.
  bool pinned = false;
  for (auto child_id :
       grouped_notification_list_->GetGroupedNotificationsForParent(
           parent_id)) {
    auto* child_notification = message_center->FindNotificationById(child_id);
    if (child_notification && child_notification->pinned()) {
      pinned = true;
      break;
    }
  }

  parent_notification->set_pinned(pinned);
}

void NotificationGroupingController::ReparentNotificationIfNecessary(
    message_center::Notification* notification) {
  if (!grouped_notification_list_->GroupedChildNotificationExists(
          notification->id())) {
    return;
  }
  auto* parent_notification =
      message_center::MessageCenter::Get()->FindParentNotification(
          notification);

  if (!parent_notification || parent_notification == notification) {
    grouped_notification_list_->RemoveGroupedChildNotification(
        notification->id());
    return;
  }

  // Update the group state for the `notification` if the parent notification
  // does not match the parent id in `grouped_notification_list_`. This happens
  // when a notification is updated with a different `NotifierId`.
  if (grouped_notification_list_->GetParentForChild(notification->id()) !=
      parent_notification->id()) {
    grouped_notification_list_->RemoveGroupedChildNotification(
        notification->id());
    grouped_notification_list_->AddGroupedNotification(
        notification->id(), parent_notification->id());
  }
}

}  // namespace ash
