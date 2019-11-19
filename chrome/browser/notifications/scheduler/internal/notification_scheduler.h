// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/scheduler/public/impression_detail.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"

namespace notifications {

class NotificationSchedulerContext;
struct NotificationParams;

// Provides notification scheduling and throttling functionalities. This class
// glues all the subsystems together for notification scheduling system.
class NotificationScheduler
    : public NotificationBackgroundTaskScheduler::Handler,
      public UserActionHandler {
 public:
  using InitCallback = base::OnceCallback<void(bool)>;
  static std::unique_ptr<NotificationScheduler> Create(
      std::unique_ptr<NotificationSchedulerContext> context);

  NotificationScheduler();
  ~NotificationScheduler() override;

  // Initializes the scheduler.
  virtual void Init(InitCallback init_callback) = 0;

  // Schedules a notification to show in the future. Throttling logic may apply
  // based on |notification_params|.
  virtual void Schedule(
      std::unique_ptr<NotificationParams> notification_params) = 0;

  // Queries impression detail for a given |SchedulerClientType|.
  virtual void GetImpressionDetail(
      SchedulerClientType type,
      ImpressionDetail::ImpressionDetailCallback callback) = 0;

  // Deletes all notifications of a given |SchedulerClientType|.
  virtual void DeleteAllNotifications(SchedulerClientType type) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationScheduler);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULER_H_
