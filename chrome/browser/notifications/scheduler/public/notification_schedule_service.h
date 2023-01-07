// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULE_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULE_SERVICE_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/public/client_overview.h"
#include "chrome/browser/notifications/scheduler/public/impression_detail.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "components/keyed_service/core/keyed_service.h"

namespace notifications {

class UserActionHandler;
struct NotificationParams;

// Service to schedule a notification to display in the future. An internal
// throttling mechanism will be applied to limit the maximum notification shown
// to the user. Also user's interaction with the notification will affect the
// frequency of notification delivery.
class NotificationScheduleService : public KeyedService {
 public:
  // Schedules a notification to display.
  virtual void Schedule(
      std::unique_ptr<NotificationParams> notification_params) = 0;

  // Deletes notifications of a given |SchedulerClientType|.
  virtual void DeleteNotifications(SchedulerClientType type) = 0;

  // Queries an overview of notifications for a given
  // |SchedulerClientType| including impression details.
  virtual void GetClientOverview(
      SchedulerClientType type,
      ClientOverview::ClientOverviewCallback callback) = 0;

  // Returns NotificationBackgroundTaskScheduler Handler.
  virtual NotificationBackgroundTaskScheduler::Handler*
  GetBackgroundTaskSchedulerHandler() = 0;

  // Returns the user action handler to process notification events.
  virtual UserActionHandler* GetUserActionHandler() = 0;

  NotificationScheduleService(const NotificationScheduleService&) = delete;
  NotificationScheduleService& operator=(const NotificationScheduleService&) =
      delete;
  ~NotificationScheduleService() override = default;

 protected:
  NotificationScheduleService() = default;
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_PUBLIC_NOTIFICATION_SCHEDULE_SERVICE_H_
