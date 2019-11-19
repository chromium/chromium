// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {
struct NotificationData;
namespace stats {

// Events to track behavior of the background task used by notification
// scheduling system. Used in histograms, don't reuse or delete values. Needs to
// match NotificationSchedulerBackgroundTaskEvent in enums.xml.
enum class BackgroundTaskEvent {
  // Background task starts.
  kStart = 0,
  // Background task finishes and stopped gracefully.
  kFinish = 1,
  // The background task is stopped by the OS without finishing the its job.
  kStopByOS = 2,
  kMaxValue = kStopByOS
};

// Used to log events for inline helpful/unhelful button events. Don't reuse or
// delete values. Needs to match NotificationSchedulerActionButtonEvent in
// enums.xml.
enum class ActionButtonEvent {
  // The notification is shown with IHNR buttons.
  kShown = 0,
  // Clicks on the helpful button.
  kHelpfulClick = 1,
  // Clicks on the unhelpful button.
  kUnhelpfulClick = 2,
  kMaxValue = kUnhelpfulClick
};

// Used to log events in impression tracker. Don't reuse or delete values. Needs
// to match NotificationSchedulerImpressionEvent in enums.xml.
enum class ImpressionEvent {
  // A new suppression is created to stop certain type of notification to show.
  kNewSuppression = 0,
  // The suppression is released due to new user action.
  kSuppressionRelease = 1,
  // The suppression is expired.
  kSuppressionExpired = 2,
  kMaxValue = kSuppressionExpired
};

// Event to track the life cycle of a scheduled notification. Don't reuse or
// delete values. Needs to match NotificationSchedulerNotificationLifeCycleEvent
// in enums.xml.
enum class NotificationLifeCycleEvent {
  // The client requests to schedule the notification.
  kScheduleRequest = 0,
  // The notification is successfully scheduled.
  kScheduled = 1,
  // The notification is dropped due to invalid input parameters.
  kInvalidInput = 2,
  // The notification is shown to the user.
  kShown = 3,
  // The notification is canceled by the client before showing the notification.
  kClientCancel = 4,
  kMaxValue = kClientCancel
};

// Enum to distinguish different databases used in notification scheduling
// system.
enum class DatabaseType {
  kImpressionDb = 0,
  kNotificationDb = 1,
  kIconDb = 2,
};

// Logs the user action when the user interacts with notification sent from the
// scheduling system.
void LogUserAction(const UserActionData& user_action_data);

// Logs events to track the behavior of the background task used by notification
// scheduling system.
void LogBackgroundTaskEvent(BackgroundTaskEvent event);

// Logs the number of notification shown in the current background task.
void LogBackgroundTaskNotificationShown(int shown_count);

// Logs database initialization result and the number of records in the
// database.
void LogDbInit(DatabaseType type, bool success, int entry_count);

// Logs the database operation result.
void LogDbOperation(DatabaseType type, bool success);

// Logs the number of impression data in the impression database.
void LogImpressionCount(int impression_count, SchedulerClientType type);

// Logs user impression events for notification scheduling system.
void LogImpressionEvent(ImpressionEvent event);

// Logs metrics before showing the notification.
void LogNotificationShow(const NotificationData& notification_data,
                         SchedulerClientType client_type);

// Logs scheduled notification life cycle event.
void LogNotificationLifeCycleEvent(NotificationLifeCycleEvent event,
                                   SchedulerClientType client_type);

// Logs png icon converter encode result.
void LogPngIconConverterEncodeResult(bool success);

// Logs png icon converter decode result.
void LogPngIconConverterDecodeResult(bool success);

}  // namespace stats
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_
