// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_ui_controller.h"

#include <memory>

#include "ash/shell.h"
#include "ash/system/message_center/metrics_utils.h"
#include "base/check.h"
#include "base/observer_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {

namespace {
// The duration used to log the number of notifications shown
// right after a user logs in.
constexpr base::TimeDelta kLoginNotificationLogDuration = base::Minutes(1);
}  // namespace

MessageCenterUiController::MessageCenterUiController(
    MessageCenterUiDelegate* delegate)
    : message_center_(message_center::MessageCenter::Get()),
      message_center_visible_(false),
      popups_visible_(false),
      delegate_(delegate) {
  message_center_->AddObserver(this);
  session_observer_.Observe(Shell::Get()->session_controller());
}

MessageCenterUiController::~MessageCenterUiController() {
  message_center_->RemoveObserver(this);
  session_observer_.Reset();
}

bool MessageCenterUiController::ShowMessageCenterBubble() {
  if (message_center_visible_)
    return true;

  HidePopupBubbleInternal();

  message_center_->SetVisibility(message_center::VISIBILITY_MESSAGE_CENTER);
  message_center_visible_ = delegate_->ShowMessageCenter();
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

void MessageCenterUiController::HidePopupBubbleInternal() {
  if (!popups_visible_)
    return;

  // NOTE: This doesn't actually hide popups, but does reset the baseline for
  // popups (for example, if the popups were moved up out of the way of a volume
  // slider).
  delegate_->HidePopups();
  popups_visible_ = false;
}

void MessageCenterUiController::OnNotificationAdded(
    const std::string& notification_id) {
  // QsRevamp handles metrics via `NotificationMetricsRecorder`.
  if (!features::IsQsRevampEnabled()) {
    metrics_utils::LogNotificationAdded(notification_id);
  }
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
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (popups_visible_)
    OnMessageCenterChanged();

  // QsRevamp handles metrics via `NotificationMetricsRecorder`.
  if (!features::IsQsRevampEnabled()) {
    // Note: we use `message_center_visible_` instead of `popups_visible_` here
    // due to timing issues when dismissing the last popup notification.
    bool is_popup = !message_center_visible_;
    if (reply.has_value()) {
      metrics_utils::LogInlineReplySent(notification_id, is_popup);
    } else if (button_index.has_value()) {
      metrics_utils::LogClickedActionButton(notification_id, is_popup);
    } else {
      metrics_utils::LogClickedBody(notification_id, is_popup);
    }
  }
}

void MessageCenterUiController::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  // QsRevamp handles metrics via `NotificationMetricsRecorder`.
  if (!features::IsQsRevampEnabled() &&
      login_notification_logging_timer_.IsRunning()) {
    notifications_displayed_in_first_minute_count_++;
  }

  NotifyUiControllerChanged();
}

void MessageCenterUiController::OnQuietModeChanged(bool in_quiet_mode) {
  NotifyUiControllerChanged();
}

void MessageCenterUiController::OnBlockingStateChanged(
    message_center::NotificationBlocker* blocker) {
  OnMessageCenterChanged();
}

void MessageCenterUiController::OnNotificationPopupShown(
    const std::string& notification_id,
    bool mark_notification_as_read) {
  // QsRevamp handles metrics via `NotificationMetricsRecorder`.
  // Timed out popup notifications are not marked as read.
  if (!features::IsQsRevampEnabled() && !mark_notification_as_read) {
    metrics_utils::LogPopupExpiredToTray(notification_id);
  }
}

void MessageCenterUiController::OnMessageViewHovered(
    const std::string& notification_id) {
  // QsRevamp handles metrics via `NotificationMetricsRecorder`.
  if (!features::IsQsRevampEnabled()) {
    // Note: we use `message_center_visible_` instead of `popups_visible_` here
    // due to timing issues when dismissing the last popup notification.
    bool is_popup = !message_center_visible_;
    metrics_utils::LogHover(notification_id, is_popup);
  }
}

void MessageCenterUiController::OnFirstSessionStarted() {
  // QsRevamp handles metrics via `NotificationMetricsRecorder`.
  if (features::IsQsRevampEnabled()) {
    return;
  }
  login_notification_logging_timer_.Start(
      FROM_HERE, kLoginNotificationLogDuration, this,
      &MessageCenterUiController::OnLoginTimerEnded);
}

void MessageCenterUiController::OnLoginTimerEnded() {
  DCHECK(!features::IsQsRevampEnabled());
  metrics_utils::LogNotificationsShownInFirstMinute(
      notifications_displayed_in_first_minute_count_);
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
