// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_controller.h"

#include "ash/constants/ash_features.h"
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

  std::vector<message_center::Notification*> unpinned_notifications,
      pinned_notifications;

  for (auto* notification : notifications) {
    (notification->pinned() ? pinned_notifications : unpinned_notifications)
        .push_back(notification);
  }

  auto pinned_notification_list_view =
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .Build();
  pinned_notification_list_view->SetDefault(views::kMarginsKey,
                                            NotificationListDefaultMargins);
  pinned_notification_list_view_ = pinned_notification_list_view.get();

  pinned_notification_list_view_tracker_.SetView(
      pinned_notification_list_view_);
  pinned_notification_list_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      [](raw_ptr<views::View>& pinned_notification_list_view) {
        pinned_notification_list_view = nullptr;
      },
      std::ref(pinned_notification_list_view_)));

  // TODO(b/322835713): Also create the unpinned notification list view from the
  // controller instead of from `NotificationCenterView`.
  for (auto* notification : pinned_notifications) {
    AddNotificationChildView(notification);
  }
  UpdateListViewBorders(/*pinned=*/true, /*force_update=*/true);

  notification_center_view_->Init(unpinned_notifications,
                                  std::move(pinned_notification_list_view));
}

void NotificationCenterController::OnNotificationAdded(const std::string& id) {
  if (!notification_center_view_) {
    return;
  }

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification) {
    return;
  }

  if (!features::AreOngoingProcessesEnabled() || !notification->pinned()) {
    notification_center_view_->OnNotificationAdded(id);
    return;
  }

  // TODO(b/322835713): Also create and manage unpinned notification views from
  // this controller instead of from `NotificationListView`.
  if (!pinned_notification_list_view_) {
    return;
  }

  if (GetMessageViewContainerById(id, pinned_notification_list_view_)) {
    OnNotificationUpdated(id);
    return;
  }

  AddNotificationChildView(notification);
  UpdateListViewBorders(notification->pinned());
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

  // TODO(b/322835713): Also create and manage unpinned notification views from
  // this controller instead of from `NotificationListView`.
  if (!pinned_notification_list_view_) {
    return;
  }

  auto* message_view_container =
      GetMessageViewContainerById(id, pinned_notification_list_view_);
  if (!message_view_container) {
    return;
  }

  pinned_notification_list_view_->RemoveChildView(message_view_container);
  UpdateListViewBorders(/*pinned=*/true);
  notification_center_view_->ListPreferredSizeChanged();
}

void NotificationCenterController::OnNotificationUpdated(
    const std::string& id) {
  if (!notification_center_view_) {
    return;
  }

  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification) {
    return;
  }

  if (!features::AreOngoingProcessesEnabled() || !notification->pinned()) {
    notification_center_view_->OnNotificationUpdated(id);
    return;
  }

  // TODO(b/322835713): Also create and manage unpinned notification views from
  // this controller instead of from `NotificationListView`.
  if (!pinned_notification_list_view_) {
    return;
  }

  auto* message_view_container =
      GetMessageViewContainerById(id, pinned_notification_list_view_);
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
    const bool pinned,
    const bool force_update) {
  auto list_view = pinned ? pinned_notification_list_view_ : nullptr;
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
  // TODO(b/322835713): Also create unpinned notifications from this controller.
  CHECK(features::AreOngoingProcessesEnabled());

  auto message_view =
      MessageViewFactory::Create(notification, /*shown_in_popup=*/false);
  message_view->SetIsNested();
  notification_center_view_->ConfigureMessageView(message_view.get());
  return message_view;
}

void NotificationCenterController::AddNotificationChildView(
    message_center::Notification* notification) {
  auto list_view =
      notification->pinned() ? pinned_notification_list_view_ : nullptr;
  if (!list_view) {
    return;
  }

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

// TODO(b/322835713): Use a `ViewModel` for `MessageViewContainer` lookups.
MessageViewContainer* NotificationCenterController::GetMessageViewContainerById(
    const std::string& id,
    views::View* list_view) {
  const auto i = base::ranges::find(
      list_view->children(), id,
      [](const views::View* v) { return AsMVC(v)->GetNotificationId(); });
  return (i == list_view->children().cend()) ? nullptr : AsMVC(*i);
}

}  // namespace ash
