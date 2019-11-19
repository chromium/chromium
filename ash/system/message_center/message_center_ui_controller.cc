// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_ui_controller.h"

#include <memory>

#include "base/observer_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

MessageCenterUiController::MessageCenterUiController(
    MessageCenterUiDelegate* delegate)
    : message_center_(message_center::MessageCenter::Get()),
      message_center_visible_(false),
      popups_visible_(false),
      delegate_(delegate) {
  message_center_->AddObserver(this);
}

MessageCenterUiController::~MessageCenterUiController() {
  message_center_->RemoveObserver(this);
}

bool MessageCenterUiController::ShowMessageCenterBubble(bool show_by_click) {
  if (message_center_visible_)
    return true;

  HidePopupBubbleInternal();

  message_center_->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  message_center_visible_ = delegate_->ShowMessageCenter(show_by_click);
  if (message_center_visible_)
    NotifyUiControllerChanged();
  return message_center_visible_;
}

bool MessageCenterUiController::HideMessageCenterBubble() {
  if (!message_center_visible_)
    return false;

  delegate_->HideMessageCenter();
  MarkMessageCenterHidden();

  return true;
}

void MessageCenterUiController::MarkMessageCenterHidden() {
  if (!message_center_visible_)
    return;
  message_center_visible_ = false;
  message_center_->SetVisibility(message_center::VISIBILITY_TRANSIENT);

  // Some notifications (like system ones) should appear as popups again
  // after the message center is closed.
  if (message_center_->HasPopupNotifications()) {
    ShowPopupBubble();
    return;
  }

  NotifyUiControllerChanged();
}

void MessageCenterUiController::ShowPopupBubble() {
  if (message_center_visible_)
    return;

  if (popups_visible_) {
    NotifyUiControllerChanged();
    return;
  }

  if (!message_center_->HasPopupNotifications())
    return;

  popups_visible_ = delegate_->ShowPopups();

  NotifyUiControllerChanged();
}

bool MessageCenterUiController::HidePopupBubble() {
  if (!popups_visible_)
    return false;
  HidePopupBubbleInternal();
  NotifyUiControllerChanged();

  return true;
}

void MessageCenterUiController::HidePopupBubbleInternal() {
  if (!popups_visible_)
    return;

  delegate_->HidePopups();
  popups_visible_ = false;
}

void MessageCenterUiController::OnNotificationAdded(
    const std::string& notification_id) {
  OnMessageCenterChanged();
}

void MessageCenterUiController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  OnMessageCenterChanged();
}

void MessageCenterUiController::OnNotificationUpdated(
    const std::string& notification_id) {
  OnMessageCenterChanged();
}

void MessageCenterUiController::OnNotificationClicked(
    const std::string& notification_id,
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (popups_visible_)
    OnMessageCenterChanged();
}

void MessageCenterUiController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  NotifyUiControllerChanged();
}

void MessageCenterUiController::OnQuietModeChanged(bool in_quiet_mode) {
  NotifyUiControllerChanged();
}

void MessageCenterUiController::OnBlockingStateChanged(
    message_center::NotificationBlocker* blocker) {
  OnMessageCenterChanged();
}

void MessageCenterUiController::OnMessageCenterChanged() {
  if (hide_on_last_notification_ && message_center_visible_ &&
      message_center_->NotificationCount() == 0) {
    HideMessageCenterBubble();
    return;
  }

  if (popups_visible_ && !message_center_->HasPopupNotifications())
    HidePopupBubbleInternal();
  else if (!popups_visible_ && message_center_->HasPopupNotifications())
    ShowPopupBubble();

  NotifyUiControllerChanged();
}

void MessageCenterUiController::NotifyUiControllerChanged() {
  delegate_->OnMessageCenterContentsChanged();
}

}  // namespace ash
