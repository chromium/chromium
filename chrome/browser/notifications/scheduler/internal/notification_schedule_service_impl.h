// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "chrome/browser/notifications/scheduler/public/user_action_handler.h"

namespace notifications {

class NotificationScheduler;
struct NotificationParams;

class NotificationScheduleServiceImpl
    : public NotificationScheduleService,
      public NotificationBackgroundTaskScheduler::Handler,
      public UserActionHandler {
 public:
  explicit NotificationScheduleServiceImpl(
      std::unique_ptr<NotificationScheduler> scheduler);
  ~NotificationScheduleServiceImpl() override;

 private:
  // NotificationScheduleService implementation.
  void Schedule(
      std::unique_ptr<NotificationParams> notification_params) override;
  void DeleteNotifications(SchedulerClientType type) override;
  void GetImpressionDetail(
      SchedulerClientType,
      ImpressionDetail::ImpressionDetailCallback callback) override;
  NotificationBackgroundTaskScheduler::Handler*
  GetBackgroundTaskSchedulerHandler() override;
  UserActionHandler* GetUserActionHandler() override;

  // NotificationBackgroundTaskScheduler::Handler implementation.
  void OnStartTask(TaskFinishedCallback callback) override;
  void OnStopTask() override;

  // UserActionHandler implementation.
  void OnUserAction(const UserActionData& action_data) override;

  // Called after initialization is done.
  void OnInitialized(bool success);

  // Provides the actual notification scheduling functionalities.
  std::unique_ptr<NotificationScheduler> scheduler_;

  base::WeakPtrFactory<NotificationScheduleServiceImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NotificationScheduleServiceImpl);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_NOTIFICATION_SCHEDULE_SERVICE_IMPL_H_
