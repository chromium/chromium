// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_bubble.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/notification_center_controller.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/message_view_container.h"
#include "ash/system/notification_center/views/notification_center_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_utils.h"
#include "chromeos/constants/chromeos_features.h"
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
  if (chromeos::features::IsNotificationWidthIncreaseEnabled()) {
    init_params.preferred_width =
        GetNotificationInMessageCenterWidth() + (2 * kMessageCenterPadding);
  }

  init_params.set_can_activate_on_click_or_tap = true;
  if (!features::IsBubbleCornerRadiusUpdateEnabled()) {
    init_params.corner_radius = kNotificationCenterBubbleCornerRadius;
  }
  bubble_view_ = std::make_unique<TrayBubbleView>(init_params);
  bubble_view_->SetMaxHeight(CalculateMaxTrayBubbleHeight(
      notification_center_tray_->GetBubbleWindowContainer()));

  if (features::IsNotificationCenterControllerEnabled()) {
    notification_center_controller_ =
        std::make_unique<NotificationCenterController>();
    bubble_view_->AddChildView(
        notification_center_controller_->CreateNotificationCenterView());
  } else {
    notification_center_view_ =
        bubble_view_->AddChildView(std::make_unique<NotificationCenterView>());
  }

  bubble_wrapper_ =
      std::make_unique<TrayBubbleWrapper>(notification_center_tray_);
}

NotificationCenterBubble::~NotificationCenterBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();
}

void NotificationCenterBubble::ShowBubble() {
  if (features::IsNotificationCenterControllerEnabled()) {
    notification_center_controller_->InitNotificationCenterView();
  }
  bubble_wrapper_->ShowBubble(std::move(bubble_view_));
  if (!features::IsNotificationCenterControllerEnabled()) {
    notification_center_view_->Init();
  }
  GetBubbleView()->SizeToContents();
}

TrayBubbleView* NotificationCenterBubble::GetBubbleView() {
  return bubble_wrapper_->bubble_view();
}

views::Widget* NotificationCenterBubble::GetBubbleWidget() {
  return bubble_wrapper_->GetBubbleWidget();
}

NotificationCenterView* NotificationCenterBubble::GetNotificationCenterView() {
  return features::IsNotificationCenterControllerEnabled()
             ? notification_center_controller_->notification_center_view()
             : notification_center_view_.get();
}

const MessageViewContainer*
NotificationCenterBubble::GetOngoingProcessMessageViewContainerById(
    const std::string& id) {
  // The controller currently handles only ongoing process notifications. To
  // access other notifications use `NotificationListView`.
  // TODO(b/322835713): Have the controller create other notification views
  // and deprecate `NotificationListView`.
  return features::AreOngoingProcessesEnabled()
             ? notification_center_controller_
                   ->GetOngoingProcessMessageViewContainerById(id)
             : nullptr;
}

void NotificationCenterBubble::UpdateBubbleBounds() {
  auto* bubble_view = GetBubbleView();
  bubble_view->SetMaxHeight(CalculateMaxTrayBubbleHeight(
      notification_center_tray_->GetBubbleWindowContainer()));
  bubble_view->ChangeAnchorRect(
      notification_center_tray_->shelf()->GetSystemTrayAnchorRect());
}

void NotificationCenterBubble::OnDidApplyDisplayChanges() {
  UpdateBubbleBounds();
}

}  // namespace ash
