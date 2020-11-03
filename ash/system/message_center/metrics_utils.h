// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_METRICS_UTILS_H_
#define ASH_SYSTEM_MESSAGE_CENTER_METRICS_UTILS_H_

#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace metrics_utils {

// Keep in sync with enum in tools/metrics/histograms/enums.xml.
enum NotificationTypeDetailed : int {
  CHROME_APP_PRIORITY_MINUS_TWO = 0,
  CHROME_APP_PRIORITY_MINUS_TWO_REQUIRE_INTERACTION = 1,
  CHROME_APP_PRIORITY_MINUS_ONE = 2,
  CHROME_APP_PRIORITY_MINUS_ONE_REQUIRE_INTERACTION = 3,
  CHROME_APP_PRIORITY_ZERO = 4,
  CHROME_APP_PRIORITY_ZERO_REQUIRE_INTERACTION = 5,
  CHROME_APP_PRIORITY_ONE = 6,
  CHROME_APP_PRIORITY_ONE_REQUIRE_INTERACTION = 7,
  CHROME_APP_PRIORITY_TWO = 8,
  CHROME_APP_PRIORITY_TWO_REQUIRE_INTERACTION = 9,
  ARC_PRIORITY_MINUS_TWO = 10,
  ARC_PRIORITY_MINUS_TWO_PINNED = 11,
  ARC_PRIORITY_MINUS_ONE = 12,
  ARC_PRIORITY_MINUS_ONE_PINNED = 13,
  ARC_PRIORITY_ZERO = 14,
  ARC_PRIORITY_ZERO_PINNED = 15,
  ARC_PRIORITY_ONE = 16,
  ARC_PRIORITY_ONE_PINNED = 17,
  ARC_PRIORITY_TWO = 18,
  ARC_PRIORITY_TWO_PINNED = 19,
  WEB = 20,
  WEB_REQUIRE_INTERACTION = 21,
  CROS_PRIORITY_MINUS_TWO = 22,
  CROS_PRIORITY_MINUS_TWO_PINNED = 23,
  CROS_PRIORITY_MINUS_ONE = 24,
  CROS_PRIORITY_MINUS_ONE_PINNED = 25,
  CROS_PRIORITY_ZERO = 26,
  CROS_PRIORITY_ZERO_PINNED = 27,
  CROS_PRIORITY_ONE = 28,
  CROS_PRIORITY_ONE_PINNED = 29,
  CROS_PRIORITY_TWO = 30,
  CROS_PRIORITY_TWO_PINNED = 31,
  CROS_SYSTEM_PRIORITY = 32,
  CROS_SYSTEM_PRIORITY_PINNED = 33,
  CROS_SYSTEM_PRIORITY_WARNING = 34,
  CROS_SYSTEM_PRIORITY_WARNING_PINNED = 35,
  CROS_SYSTEM_PRIORITY_CRITICAL_WARNING = 36,
  CROS_SYSTEM_PRIORITY_CRITICAL_WARNING_PINNED = 37,
  OTHER = 38,
  kMaxValue = OTHER,
};

// Returns the detailed notification type enum for a notification.
NotificationTypeDetailed GetNotificationType(
    const message_center::Notification& notification);

// Returns the detailed notification type enum for a notification id.
base::Optional<NotificationTypeDetailed> GetNotificationType(
    const std::string& notification_id);

// Logs a ClickedBody event.
void LogClickedBody(const std::string& notification_id, bool is_popup);

// Logs a ClickedActionButton event.
void LogClickedActionButton(const std::string& notification_id, bool is_popup);

// Logs a ExpireToTray event for a pop-up notification.
void LogPopupExpiredToTray(const std::string& notification_id);

// Logs a ClosedByUser event.
void LogClosedByUser(const std::string& notification_id,
                     bool is_swipe,
                     bool is_popup);

// Logs a SettingsShown event.
void LogSettingsShown(const std::string& notification_id,
                      bool is_slide_controls,
                      bool is_popup);

// Logs a Snoozed event.
void LogSnoozed(const std::string& notification_id,
                bool is_slide_controls,
                bool is_popup);

// Logs a popup Shown event.
void LogPopupShown(const std::string& notification_id);

// Logs a tray ClosedByClearAll event.
void LogClosedByClearAll(const std::string& notification_id);

// Logs a notification added event.
void LogNotificationAdded(const std::string& notification_id);

}  // namespace metrics_utils

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_METRICS_UTILS_H_
