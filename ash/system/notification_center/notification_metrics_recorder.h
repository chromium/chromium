// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_METRICS_RECORDER_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/message_center/message_center_observer.h"

namespace ash {

class NotificationCenterTray;

// NotificationMetricsRecorder records metrics about user interactions with
// notifications.
// NOTE: This class is tested by MessageCenterMetricsUtilsTest.
class ASH_EXPORT NotificationMetricsRecorder
    : public message_center::MessageCenterObserver,
      public SessionObserver {
 public:
  explicit NotificationMetricsRecorder(NotificationCenterTray* tray);
  NotificationMetricsRecorder(const NotificationMetricsRecorder&) = delete;
  NotificationMetricsRecorder& operator=(const NotificationMetricsRecorder&) =
      delete;
  ~NotificationMetricsRecorder() override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationClicked(
      const std::string& notification_id,
      const std::optional<int>& button_index,
      const std::optional<std::u16string>& reply) override;
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;
  void OnNotificationPopupShown(const std::string& notification_id,
                                bool mark_notification_as_read) override;
  void OnMessageViewHovered(const std::string& notification_id) override;

  // SessionObserver:
  void OnFirstSessionStarted() override;

 private:
  // Callback for `login_notification_logging_timer_`.
  void OnLoginTimerEnded();

  // Returns whether the notification center bubble is visible.
  bool IsNotificationCenterVisible() const;

  const raw_ptr<NotificationCenterTray> tray_;
  int notifications_displayed_in_first_minute_count_ = 0;
  base::OneShotTimer login_notification_logging_timer_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_NOTIFICATION_METRICS_RECORDER_H_
