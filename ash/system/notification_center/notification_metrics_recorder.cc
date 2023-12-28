// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_metrics_recorder.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "base/check.h"
#include "base/time/time.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

// Duration used to log the number of notifications shown after a user logs in.
constexpr base::TimeDelta kLoginNotificationLogDuration = base::Minutes(1);

}  // namespace

NotificationMetricsRecorder::NotificationMetricsRecorder(
    NotificationCenterTray* tray)
    : tray_(tray) {
  CHECK(tray_);
  message_center::MessageCenter::Get()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
}

NotificationMetricsRecorder::~NotificationMetricsRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void NotificationMetricsRecorder::OnNotificationAdded(
    const std::string& notification_id) {
  metrics_utils::LogNotificationAdded(notification_id);
}

void NotificationMetricsRecorder::OnNotificationClicked(
    const std::string& notification_id,
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  bool is_popup = !IsNotificationCenterVisible();
  if (reply.has_value()) {
    metrics_utils::LogInlineReplySent(notification_id, is_popup);
  } else if (button_index.has_value()) {
    metrics_utils::LogClickedActionButton(notification_id, is_popup,
                                          button_index.value());
  } else {
    metrics_utils::LogClickedBody(notification_id, is_popup);
  }
}

void NotificationMetricsRecorder::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  if (login_notification_logging_timer_.IsRunning()) {
    notifications_displayed_in_first_minute_count_++;
  }
}

void NotificationMetricsRecorder::OnNotificationPopupShown(
    const std::string& notification_id,
    bool mark_notification_as_read) {
  // Timed out popup notifications are not marked as read.
  if (!mark_notification_as_read) {
    metrics_utils::LogPopupExpiredToTray(notification_id);
  }
}

void NotificationMetricsRecorder::OnMessageViewHovered(
    const std::string& notification_id) {
  metrics_utils::LogHover(notification_id,
                          /*is_popup=*/!IsNotificationCenterVisible());
}

void NotificationMetricsRecorder::OnFirstSessionStarted() {
  login_notification_logging_timer_.Start(
      FROM_HERE, kLoginNotificationLogDuration, this,
      &NotificationMetricsRecorder::OnLoginTimerEnded);
}

void NotificationMetricsRecorder::OnLoginTimerEnded() {
  metrics_utils::LogNotificationsShownInFirstMinute(
      notifications_displayed_in_first_minute_count_);
}

bool NotificationMetricsRecorder::IsNotificationCenterVisible() const {
  return tray_->IsBubbleShown();
}

}  // namespace ash
