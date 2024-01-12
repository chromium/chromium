// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"

namespace ash {

NotificationCenterController::NotificationCenterController() {
  CHECK(features::IsNotificationCenterControllerEnabled());
}

NotificationCenterController::~NotificationCenterController() = default;

std::unique_ptr<views::View> NotificationCenterController::CreateView() {
  auto notification_center_view = std::make_unique<NotificationCenterView>();
  notification_center_view_tracker_.SetView(notification_center_view.get());
  return std::move(notification_center_view);
}

void NotificationCenterController::InitView() {
  auto* notification_center_view = GetNotificationCenterView();
  CHECK(notification_center_view);

  auto notifications =
      message_center_utils::GetSortedNotificationsWithOwnView();
  notification_center_view->Init(notifications);
}

NotificationCenterView*
NotificationCenterController::GetNotificationCenterView() {
  return static_cast<NotificationCenterView*>(
      notification_center_view_tracker_.view());
}

}  // namespace ash
