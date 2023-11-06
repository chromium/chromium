// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_bubble.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kNotificationCenterBubbleCornerRadius = 24;

}  // namespace

NotificationCenterBubble::NotificationCenterBubble(
    NotificationCenterTray* notification_center_tray)
    : notification_center_tray_(notification_center_tray) {
  auto init_params = CreateInitParamsForTrayBubble(
      /*tray=*/notification_center_tray_, /*anchor_to_shelf_corner=*/true);

  // Create and customize bubble view.
  init_params.corner_radius = kNotificationCenterBubbleCornerRadius;
  bubble_view_ = std::make_unique<TrayBubbleView>(init_params);
  bubble_view_->SetMaxHeight(CalculateMaxTrayBubbleHeight(
      notification_center_tray_->GetBubbleWindowContainer()));

  notification_center_view_ =
      bubble_view_->AddChildView(std::make_unique<NotificationCenterView>());

  bubble_wrapper_ =
      std::make_unique<TrayBubbleWrapper>(notification_center_tray_);
}

NotificationCenterBubble::~NotificationCenterBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();
}

void NotificationCenterBubble::ShowBubble() {
  bubble_wrapper_->ShowBubble(std::move(bubble_view_));
  notification_center_view_->Init();
  GetBubbleView()->SizeToContents();
}

TrayBubbleView* NotificationCenterBubble::GetBubbleView() {
  return bubble_wrapper_->bubble_view();
}

views::Widget* NotificationCenterBubble::GetBubbleWidget() {
  return bubble_wrapper_->GetBubbleWidget();
}

void NotificationCenterBubble::UpdateBubbleBounds() {
  auto* bubble_view = GetBubbleView();
  bubble_view->SetMaxHeight(CalculateMaxTrayBubbleHeight(
      notification_center_tray_->GetBubbleWindowContainer()));
  bubble_view->ChangeAnchorRect(
      notification_center_tray_->shelf()->GetSystemTrayAnchorRect());
}

void NotificationCenterBubble::OnDisplayConfigurationChanged() {
  UpdateBubbleBounds();
}

}  // namespace ash
