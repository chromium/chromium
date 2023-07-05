// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {
struct NotificationData;
namespace stats {

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

// Logs the number of notification shown in the current background task.
void LogBackgroundTaskNotificationShown(int shown_count);

// Logs metrics before showing the notification.
void LogNotificationShow(const NotificationData& notification_data,
                         SchedulerClientType client_type);

// Logs scheduled notification life cycle event.
void LogNotificationLifeCycleEvent(NotificationLifeCycleEvent event,
                                   SchedulerClientType client_type);
}  // namespace stats
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_STATS_H_
