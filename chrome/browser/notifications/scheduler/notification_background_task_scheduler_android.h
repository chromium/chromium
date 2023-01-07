// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_ANDROID_H_

#include "base/time/time.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"

// This class contains:
// 1. Android implementation of NotificationBackgroundTaskScheduler, which
// asks Android API to schedule background task.
// 2. JNI calls to route background task events to native.
// The life cycle of this object is owned by a keyed service in native.
class NotificationBackgroundTaskSchedulerAndroid
    : public notifications::NotificationBackgroundTaskScheduler {
 public:
  NotificationBackgroundTaskSchedulerAndroid();
  NotificationBackgroundTaskSchedulerAndroid(
      const NotificationBackgroundTaskSchedulerAndroid&) = delete;
  NotificationBackgroundTaskSchedulerAndroid& operator=(
      const NotificationBackgroundTaskSchedulerAndroid&) = delete;
  ~NotificationBackgroundTaskSchedulerAndroid() override;

 private:
  // NotificationBackgroundTaskScheduler implementation.
  void Schedule(base::TimeDelta window_start,
                base::TimeDelta window_end) override;
  void Cancel() override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_ANDROID_H_
