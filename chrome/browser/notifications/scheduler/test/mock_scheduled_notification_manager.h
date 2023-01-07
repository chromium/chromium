// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_SCHEDULED_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_SCHEDULED_NOTIFICATION_MANAGER_H_

#include "chrome/browser/notifications/scheduler/internal/scheduled_notification_manager.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifications {
namespace test {

class MockScheduledNotificationManager : public ScheduledNotificationManager {
 public:
  MockScheduledNotificationManager();
  ~MockScheduledNotificationManager() override;

  MOCK_METHOD1(Init, void(base::OnceCallback<void(bool)>));
  MOCK_METHOD2(ScheduleNotification,
               void(std::unique_ptr<notifications::NotificationParams>,
                    ScheduleCallback));
  MOCK_METHOD2(DisplayNotification, void(const std::string&, DisplayCallback));
  MOCK_CONST_METHOD1(GetAllNotifications, void(Notifications*));
  MOCK_CONST_METHOD2(GetNotifications,
                     void(SchedulerClientType,
                          std::vector<const NotificationEntry*>*));
  MOCK_METHOD1(DeleteNotifications, void(SchedulerClientType));
};

}  // namespace test
}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_TEST_MOCK_SCHEDULED_NOTIFICATION_MANAGER_H_
