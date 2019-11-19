// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_BACKGROUND_TASK_COORDINATOR_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_BACKGROUND_TASK_COORDINATOR_H_

#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockBackgroundTaskCoordinator : public BackgroundTaskCoordinator {
 public:
  MockBackgroundTaskCoordinator();
  ~MockBackgroundTaskCoordinator() override;
  MOCK_METHOD2(ScheduleBackgroundTask,
               void(BackgroundTaskCoordinator::Notifications notifications,
                    BackgroundTaskCoordinator::ClientStates client_states));
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_BACKGROUND_TASK_COORDINATOR_H_
