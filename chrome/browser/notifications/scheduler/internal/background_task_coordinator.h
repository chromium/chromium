// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_BACKGROUND_TASK_COORDINATOR_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_BACKGROUND_TASK_COORDINATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace base {
class Clock;
class TimeDelta;
}  // namespace base

namespace notifications {

class NotificationBackgroundTaskScheduler;
struct ClientState;
struct NotificationEntry;
struct SchedulerConfig;

// Schedules background task at the right time based on scheduled notification
// data and impression data.
class BackgroundTaskCoordinator {
 public:
  using Notifications = std::map<
      SchedulerClientType,
      std::vector<raw_ptr<const NotificationEntry, VectorExperimental>>>;
  using ClientStates = std::map<SchedulerClientType, const ClientState*>;
  using TimeRandomizer = base::RepeatingCallback<base::TimeDelta()>;

  static base::TimeDelta DefaultTimeRandomizer(
      const base::TimeDelta& time_window);

  static std::unique_ptr<BackgroundTaskCoordinator> Create(
      std::unique_ptr<NotificationBackgroundTaskScheduler> background_task,
      const SchedulerConfig* config,
      base::Clock* clock);

  virtual ~BackgroundTaskCoordinator();

  // Schedule background task based on current notification in the storage.
  virtual void ScheduleBackgroundTask(Notifications notifications,
                                      ClientStates client_states) = 0;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_BACKGROUND_TASK_COORDINATOR_H_
