// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/notification_center/views/message_view_container.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr gfx::Insets NotificationListDefaultMargins =
    gfx::Insets::VH(kMessageListNotificationSpacing, 0);

}  // namespace

NotificationCenterController::NotificationCenterController() {
  CHECK(features::IsNotificationCenterControllerEnabled());

  auto* message_center = message_center::MessageCenter::Get();
  CHECK(message_center);
  message_center->AddObserver(this);
}

NotificationCenterController::~NotificationCenterController() {
  auto* message_center = message_center::MessageCenter::Get();
  if (message_center) {
    message_center->RemoveObserver(this);
  }
}

std::unique_ptr<views::View>
NotificationCenterController::CreateNotificationCenterView() {
  auto notification_center_view = std::make_unique<NotificationCenterView>();
  notification_center_view_ = notification_center_view.get();
  notification_center_view_tracker_.SetView(notification_center_view_);
  notification_center_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      [](raw_ptr<NotificationCenterView>& notification_center_view) {
        notification_center_view = nullptr;
      },
      std::ref(notification_center_view_)));
  return std::move(notification_center_view);
}

void NotificationCenterController::InitNotificationCenterView() {
  CHECK(notification_center_view_);

  auto notifications =
      message_center_utils::GetSortedNotificationsWithOwnView();

  if (!features::AreOngoingProcessesEnabled()) {
    notification_center_view_->Init(notifications);
    return;
  }

  // Ongoing processes include pinned system notifications exclusively.
  // Any other type of notifications, including non-pinned or non-system
  // (sourced from Web, ARC, etc.) will exist in the regular notification list.
  std::vector<message_center::Notification*> regular_notifications,
      ongoing_processes;

  for (auto* notification : notifications) {
    if (notification->notifier_id().type !=
        message_center::NotifierType::SYSTEM_COMPONENT) {
      regular_notifications.push_back(notification);
      continue;
    }

    if (notification->pinned()) {
      ongoing_processes.push_back(notification);
    } else {
      regular_notifications.push_back(notification);
    }
  }

  auto ongoing_process_list_view =
      views::Builder<views::FlexLayoutView>()
          .SetID(VIEW_ID_NOTIFICATION_BUBBLE_ONGOING_PROCESS_LIST)
          .SetOrientation(views::LayoutOrientation::kVertical)
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(views::kMarginsKey,
                               NotificationListDefaultMargins);
          }))
          .Build();
  ongoing_process_list_view_ = ongoing_process_list_view.get();

  ongoing_process_list_view_tracker_.SetView(ongoing_process_list_view_);
  ongoing_process_list_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      [](raw_ptr<views::View>& ongoing_process_list_view) {
        ongoing_process_list_view = nullptr;
      },
      std::ref(ongoing_process_list_view_)));

  // TODO(b/322835713): Also create the regular notification list view from the
  // controller instead of from `NotificationCenterView`.
  for (auto* notification : ongoing_processes) {
    AddNotificationChildView(notification);
  }
  UpdateListViewBorders(/*is_ongoing_process=*/true, /*force_update=*/true);

  notification_center_view_->Init(regular_notifications,
                                  std::move(ongoing_process_list_view));
}

MessageViewContainer*
NotificationCenterController::GetOngoingProcessMessageViewContainerById(
    const std::string& id) {
  // TODO(b/322835713): This function should search both regular notification
  // and ongoing process lists when they're all created by this controller.
  auto* list_view = ongoing_process_list_view_.get();

  const auto i = base::ranges::find(
      list_view->children(), id,
      [](const views::View* v) { return AsMVC(v)->GetNotificationId(); });
  return (i == list_view->children().cend()) ? nullptr : AsMVC(*i);
}

void NotificationCenterController::OnNotificationAdded(const std::string& id) {
  if (!features::AreOngoingProcessesEnabled() || !notification_center_view_) {
    return;
  }

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification) {
    return;
  }

  bool is_ongoing_process = notification->pinned() &&
                            notification->notifier_id().type ==
                                message_center::NotifierType::SYSTEM_COMPONENT;
  if (!is_ongoing_process) {
    // TODO(b/322835713): Also create and manage other notification views from
    // this controller instead of from `NotificationListView`.
    notification_center_view_->OnNotificationAdded(id);
    return;
  }

  if (!ongoing_process_list_view_) {
    return;
  }

  if (GetOngoingProcessMessageViewContainerById(id)) {
    OnNotificationUpdated(id);
    return;
  }

  AddNotificationChildView(notification);
  UpdateListViewBorders(is_ongoing_process);
  notification_center_view_->ListPreferredSizeChanged();
}

void NotificationCenterController::OnNotificationRemoved(const std::string& id,
                                                         bool by_user) {
  if (!notification_center_view_) {
    return;
  }

  notification_center_view_->OnNotificationRemoved(id, by_user);

  if (!features::AreOngoingProcessesEnabled()) {
    return;
  }

  // TODO(b/322835713): Also create and manage other notification views from
  // this controller instead of from `NotificationListView`.
  if (!ongoing_process_list_view_) {
    return;
  }

  auto* message_view_container = GetOngoingProcessMessageViewContainerById(id);
  if (!message_view_container) {
    return;
  }

  ongoing_process_list_view_->RemoveChildView(message_view_container);
  UpdateListViewBorders(/*is_ongoing_process=*/true);
  notification_center_view_->ListPreferredSizeChanged();
}

void NotificationCenterController::OnNotificationUpdated(
    const std::string& id) {
  if (!features::AreOngoingProcessesEnabled() || !notification_center_view_) {
    return;
  }

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification) {
    return;
  }

  bool is_ongoing_process = notification->pinned() &&
                            notification->notifier_id().type ==
                                message_center::NotifierType::SYSTEM_COMPONENT;
  if (!is_ongoing_process) {
    // TODO(b/322835713): Also manage other notification views from this
    // controller instead of from `NotificationListView`.
    notification_center_view_->OnNotificationUpdated(id);
    return;
  }

  if (!ongoing_process_list_view_) {
    return;
  }

  auto* message_view_container = GetOngoingProcessMessageViewContainerById(id);
  if (!message_view_container) {
    return;
  }

  auto previous_height = message_view_container->CalculateHeight();
  message_view_container->UpdateWithNotification(*notification);

  if (previous_height != message_view_container->CalculateHeight()) {
    notification_center_view_->ListPreferredSizeChanged();
  }
}

void NotificationCenterController::UpdateListViewBorders(
    const bool is_ongoing_process,
    const bool force_update) {
  auto list_view = is_ongoing_process ? ongoing_process_list_view_ : nullptr;
  if (!list_view) {
    return;
  }

  auto children = list_view->children();
  for (views::View* child : children) {
    AsMVC(child)->UpdateBorder(/*is_top=*/child == children.front(),
                               /*is_bottom=*/child == children.back(),
                               force_update);
  }
}

std::unique_ptr<message_center::MessageView>
NotificationCenterController::CreateMessageView(
    const message_center::Notification& notification) {
  CHECK(features::AreOngoingProcessesEnabled());

  auto message_view =
      MessageViewFactory::Create(notification, /*shown_in_popup=*/false);
  if (!notification.group_child()) {
    message_view->SetIsNested();
  }
  notification_center_view_->ConfigureMessageView(message_view.get());
  return message_view;
}

void NotificationCenterController::AddNotificationChildView(
    message_center::Notification* notification) {
  // TODO(b/322835713): Currently only handles ongoing processes. Also create
  // other notifications from this controller.
  auto list_view = ongoing_process_list_view_;

  auto message_view_container = std::make_unique<MessageViewContainer>(
      /*message_view=*/CreateMessageView(*notification));

  // The insertion order for notifications is reversed.
  list_view->AddChildViewAt(std::move(message_view_container),
                            /*index=*/list_view->children().size());

  message_center::MessageCenter::Get()->DisplayedNotification(
      notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
}

const MessageViewContainer* NotificationCenterController::AsMVC(
    const views::View* v) {
  return static_cast<const MessageViewContainer*>(v);
}

MessageViewContainer* NotificationCenterController::AsMVC(views::View* v) {
  return static_cast<MessageViewContainer*>(v);
}

}  // namespace ash
