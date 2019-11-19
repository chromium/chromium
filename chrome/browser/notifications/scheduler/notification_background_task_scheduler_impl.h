// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_IMPL_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"

// Default implementation of NotificatioNBackgroundTaskScheduler.
class NotificationBackgroundTaskSchedulerImpl
    : public notifications::NotificationBackgroundTaskScheduler {
 public:
  NotificationBackgroundTaskSchedulerImpl();
  ~NotificationBackgroundTaskSchedulerImpl() override;

 private:
  // NotificationBackgroundTaskScheduler implementation.
  void Schedule(base::TimeDelta window_start,
                base::TimeDelta window_end) override;
  void Cancel() override;

  DISALLOW_COPY_AND_ASSIGN(NotificationBackgroundTaskSchedulerImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_IMPL_H_
