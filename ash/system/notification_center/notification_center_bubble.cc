// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_bubble.h"

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/notification_center_controller.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/notification_center/views/message_view_container.h"
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
  init_params.preferred_width =
      kNotificationInMessageCenterWidth + (2 * kMessageCenterPadding);

  init_params.set_can_activate_on_click_or_tap = true;
  init_params.corner_radius = kNotificationCenterBubbleCornerRadius;
  bubble_view_ = std::make_unique<TrayBubbleView>(init_params);
  bubble_view_->SetMaxHeight(CalculateMaxTrayBubbleHeight(
      notification_center_tray_->GetBubbleWindowContainer()));

  notification_center_controller_ =
      std::make_unique<NotificationCenterController>();
  bubble_view_->AddChildView(
      notification_center_controller_->CreateNotificationCenterView());

  bubble_wrapper_ =
      std::make_unique<TrayBubbleWrapper>(notification_center_tray_);
}

NotificationCenterBubble::~NotificationCenterBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();
}

void NotificationCenterBubble::ShowBubble() {
  notification_center_controller_->InitNotificationCenterView();
  bubble_wrapper_->ShowBubble(std::move(bubble_view_));
  GetBubbleView()->SizeToContents();
}

TrayBubbleView* NotificationCenterBubble::GetBubbleView() {
  return bubble_wrapper_->bubble_view();
}

views::Widget* NotificationCenterBubble::GetBubbleWidget() {
  return bubble_wrapper_->GetBubbleWidget();
}

NotificationCenterView* NotificationCenterBubble::GetNotificationCenterView() {
  return notification_center_controller_->notification_center_view();
}

const MessageViewContainer*
NotificationCenterBubble::GetOngoingProcessMessageViewContainerById(
    const std::string& id) {
  // The controller currently handles only ongoing process notifications. To
  // access other notifications use `NotificationListView`.
  // TODO(b/322835713): Have the controller create other notification views
  // and deprecate `NotificationListView`.
  return notification_center_controller_
      ->GetOngoingProcessMessageViewContainerById(id);
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
