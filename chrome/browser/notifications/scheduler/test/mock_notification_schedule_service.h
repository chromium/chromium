// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_SCHEDULE_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_SCHEDULE_SERVICE_H_

#include <memory>

#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_schedule_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockNotificationScheduleService : public NotificationScheduleService {
 public:
  MockNotificationScheduleService();
  ~MockNotificationScheduleService() override;

  MOCK_METHOD1(Schedule,
               void(std::unique_ptr<NotificationParams> notification_params));
  MOCK_METHOD1(DeleteNotifications, void(SchedulerClientType type));
  MOCK_METHOD2(GetClientOverview,
               void(SchedulerClientType type,
                    ClientOverview::ClientOverviewCallback callback));
  MOCK_METHOD0(GetBackgroundTaskSchedulerHandler,
               NotificationBackgroundTaskScheduler::Handler*());
  MOCK_METHOD0(GetUserActionHandler, UserActionHandler*());
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_SCHEDULE_SERVICE_H_
