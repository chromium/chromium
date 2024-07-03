// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/stats.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace stats {
namespace {

void TestLogUserAction(const UserActionData& user_action_data,
                       ActionButtonEvent action_button_event) {
  base::HistogramTester histograms;
  LogUserAction(user_action_data);
}

void TestNotificationShow(const NotificationData& notification_data,
                          SchedulerClientType client_type,
                          bool expect_ihnr_histogram,
                          bool expect_life_cycle_histogram) {
  base::HistogramTester histograms;
  LogNotificationShow(notification_data, client_type);
  if (expect_life_cycle_histogram) {
    histograms.ExpectBucketCount(
        "Notifications.Scheduler.NotificationLifeCycleEvent",
        NotificationLifeCycleEvent::kShown, 1);
    histograms.ExpectBucketCount(
        "Notifications.Scheduler.NotificationLifeCycleEvent.__Test__",
        NotificationLifeCycleEvent::kShown, 1);
  } else {
    histograms.ExpectTotalCount(
        "Notifications.Scheduler.NotificationLifeCycleEvent", 0);
    histograms.ExpectTotalCount(
        "Notifications.Scheduler.NotificationLifeCycleEvent.__Test__", 0);
  }
}

// Verifies that ihnr buttons clicks are logged.
TEST(NotificationSchedulerStatsTest, LogUserActionIhnrButton) {
  UserActionData user_action_data(SchedulerClientType::kTest1,
                                  UserActionType::kButtonClick, "guid");
  user_action_data.button_click_info = ButtonClickInfo();
  user_action_data.button_click_info->type = ActionButtonType::kHelpful;
  TestLogUserAction(user_action_data, ActionButtonEvent::kHelpfulClick);

  user_action_data.button_click_info->type = ActionButtonType::kUnhelpful;
  TestLogUserAction(user_action_data, ActionButtonEvent::kUnhelpfulClick);
}

// Verifies notification show event is logged when there are ihnr buttons.
TEST(NotificationSchedulerStatsTest, LogNotificationShow) {
  NotificationData notification_data;
  notification_data.buttons.emplace_back(NotificationData::Button());

  // Notification without ihnr buttons.
  TestNotificationShow(notification_data, SchedulerClientType::kTest1,
                       false /*expect_ihnr_histogram*/,
                       true /*expect_life_cycle_histogram*/);

  std::vector<ActionButtonType> types{ActionButtonType::kHelpful,
                                      ActionButtonType::kUnhelpful};
  for (auto action_button_type : types) {
    notification_data.buttons.front().type = action_button_type;
    // Notification with ihnr buttons.
    TestNotificationShow(notification_data, SchedulerClientType::kTest1,
                         true /*expect_ihnr_histogram*/,
                         true /*expect_life_cycle_histogram*/);
  }
}

}  // namespace
}  // namespace stats
}  // namespace notifications
