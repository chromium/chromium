// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_H_

#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockNotificationBackgroundTaskScheduler
    : public NotificationBackgroundTaskScheduler {
 public:
  MockNotificationBackgroundTaskScheduler();
  ~MockNotificationBackgroundTaskScheduler() override;
  MOCK_METHOD2(Schedule, void(base::TimeDelta, base::TimeDelta));
  MOCK_METHOD0(Cancel, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNotificationBackgroundTaskScheduler);
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_BACKGROUND_TASK_SCHEDULER_H_
