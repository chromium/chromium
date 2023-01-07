// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULER_CONTEXT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULER_CONTEXT_H_

#include <memory>
#include <vector>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace notifications {

class BackgroundTaskCoordinator;
class DisplayAgent;
class DisplayDecider;
class ImpressionHistoryTracker;
class NotificationSchedulerClientRegistrar;
class ScheduledNotificationManager;
struct SchedulerConfig;

// Context that contains necessary components needed by the notification
// scheduler to perform tasks.
class NotificationSchedulerContext {
 public:
  NotificationSchedulerContext(
      std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar,
      std::unique_ptr<BackgroundTaskCoordinator> background_task_coordinator,
      std::unique_ptr<ImpressionHistoryTracker> impression_tracker,
      std::unique_ptr<ScheduledNotificationManager> notification_manager,
      std::unique_ptr<DisplayAgent> display_agent,
      std::unique_ptr<DisplayDecider> display_decider,
      std::unique_ptr<SchedulerConfig> config);
  NotificationSchedulerContext(const NotificationSchedulerContext&) = delete;
  NotificationSchedulerContext& operator=(const NotificationSchedulerContext&) =
      delete;
  ~NotificationSchedulerContext();

  NotificationSchedulerClientRegistrar* client_registrar() {
    return client_registrar_.get();
  }

  BackgroundTaskCoordinator* background_task_coordinator() {
    return background_task_coordinator_.get();
  }

  ImpressionHistoryTracker* impression_tracker() {
    return impression_tracker_.get();
  }

  ScheduledNotificationManager* notification_manager() {
    return notification_manager_.get();
  }

  DisplayAgent* display_agent() { return display_agent_.get(); }

  DisplayDecider* display_decider() { return display_decider_.get(); }

  const SchedulerConfig* config() const { return config_.get(); }

 private:
  // Holds a list of clients using the notification scheduler system.
  std::unique_ptr<NotificationSchedulerClientRegistrar> client_registrar_;

  // Tracks user impressions towards specific notification type.
  std::unique_ptr<ImpressionHistoryTracker> impression_tracker_;

  // Stores all scheduled notifications.
  std::unique_ptr<ScheduledNotificationManager> notification_manager_;

  // Default display flow to show the notification.
  std::unique_ptr<DisplayAgent> display_agent_;

  // Helper class to decide which notification should be displayed to the user.
  std::unique_ptr<DisplayDecider> display_decider_;

  // System configuration.
  std::unique_ptr<SchedulerConfig> config_;

  // Used to schedule background task in OS level.
  std::unique_ptr<BackgroundTaskCoordinator> background_task_coordinator_;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULER_CONTEXT_H_
