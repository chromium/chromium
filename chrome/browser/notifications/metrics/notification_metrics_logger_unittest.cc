// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"

#include "base/test/metrics/user_action_tester.h"
#include "content/public/common/persistent_notification_status.h"
#include "testing/gtest/include/gtest/gtest.h"

class NotificationMetricsLoggerTest : public ::testing::Test {
 protected:
  base::UserActionTester user_action_tester_;
  NotificationMetricsLogger logger_;
};

TEST_F(NotificationMetricsLoggerTest, PersistentNotificationShown) {
  logger_.LogPersistentNotificationShown();
  EXPECT_EQ(
      1, user_action_tester_.GetActionCount("Notifications.Persistent.Shown"));
}

TEST_F(NotificationMetricsLoggerTest, PersistentNotificationClosedByUser) {
  logger_.LogPersistentNotificationClosedByUser();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClosedByUser"));
}

TEST_F(NotificationMetricsLoggerTest,
       PersistentNotificationClosedProgrammatically) {
  logger_.LogPersistentNotificationClosedProgrammatically();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClosedProgrammatically"));
}

TEST_F(NotificationMetricsLoggerTest, PersistentNotificationClick) {
  logger_.LogPersistentNotificationClick();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.Clicked"));
}

TEST_F(NotificationMetricsLoggerTest, PersistentNotificationClickNoPermission) {
  logger_.LogPersistentNotificationClickWithoutPermission();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClickedWithoutPermission"));
}

TEST_F(NotificationMetricsLoggerTest, PersistentNotificationAction) {
  logger_.LogPersistentNotificationActionButtonClick();
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Notifications.Persistent.ClickedActionButton"));
}
