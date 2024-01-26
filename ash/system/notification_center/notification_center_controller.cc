// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"

namespace ash {

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

std::unique_ptr<views::View> NotificationCenterController::CreateView() {
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

void NotificationCenterController::InitView() {
  CHECK(notification_center_view_);
  auto notifications =
      message_center_utils::GetSortedNotificationsWithOwnView();
  notification_center_view_->Init(notifications);
}

void NotificationCenterController::OnNotificationAdded(const std::string& id) {
  if (!notification_center_view_) {
    return;
  }

  notification_center_view_->OnNotificationAdded(id);
}

void NotificationCenterController::OnNotificationRemoved(const std::string& id,
                                                         bool by_user) {
  if (!notification_center_view_) {
    return;
  }

  notification_center_view_->OnNotificationRemoved(id, by_user);
}

void NotificationCenterController::OnNotificationUpdated(
    const std::string& id) {
  if (!notification_center_view_) {
    return;
  }

  notification_center_view_->OnNotificationUpdated(id);
}

}  // namespace ash
