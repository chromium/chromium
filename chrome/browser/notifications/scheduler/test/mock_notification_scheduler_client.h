// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_SCHEDULER_CLIENT_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_SCHEDULER_CLIENT_H_

#include <memory>
#include <set>
#include <string>

#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockNotificationSchedulerClient : public NotificationSchedulerClient {
 public:
  MockNotificationSchedulerClient();
  ~MockNotificationSchedulerClient() override;
  MOCK_METHOD2(BeforeShowNotification,
               void(std::unique_ptr<NotificationData>,
                    NotificationDataCallback));
  MOCK_METHOD2(OnSchedulerInitialized, void(bool, std::set<std::string>));
  MOCK_METHOD1(OnUserAction, void(const UserActionData&));
  MOCK_METHOD1(GetThrottleConfig, void(ThrottleConfigCallback));
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_NOTIFICATION_SCHEDULER_CLIENT_H_
