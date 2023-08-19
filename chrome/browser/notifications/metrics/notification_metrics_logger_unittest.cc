// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom.h"

class NotificationMetricsLoggerTest : public ::testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  NotificationMetricsLogger logger_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager manager_{TestingBrowserProcess::GetGlobal()};
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

TEST_F(NotificationMetricsLoggerTest, PersistentNotificationSize) {
  ASSERT_TRUE(manager_.SetUp());
  TestingProfile* profile = manager_.CreateTestingProfile("test");
  safe_browsing::SetEnhancedProtectionPrefForTests(profile->GetPrefs(), true);
  blink::PlatformNotificationData notification_data;
  notification_data.title = u"Notification Title";
  notification_data.body = u"Notification body.";
  notification_data.icon = GURL("https://example.com/icon.png");
  notification_data.image = GURL("https://example.com/image.png");
  notification_data.badge = GURL("https://example.com/badge.png");
  const char data[] = "developer's data";
  notification_data.data.assign(std::begin(data), std::end(data));
  notification_data.actions.resize(2);
  notification_data.actions[0] = blink::mojom::NotificationAction::New();
  notification_data.actions[0]->icon = GURL("https://example.com/icon.png");
  notification_data.actions[0]->action = "action 0";
  notification_data.actions[0]->title = u"title 0";
  notification_data.actions[0]->placeholder = u"placeholder 0";
  notification_data.actions[1] = blink::mojom::NotificationAction::New();
  notification_data.actions[1]->icon = GURL("https://example.com/icon.png");
  notification_data.actions[1]->action = "action 1";
  notification_data.actions[1]->title = u"title 1";
  notification_data.actions[1]->placeholder = u"placeholder 1";

  logger_.LogPersistentNotificationSize(profile, notification_data,
                                        GURL("http://notification.orig.in"));

  EXPECT_EQ(28, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Origin.SizeInBytes"));
  EXPECT_EQ(18, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Title.SizeInBytes"));
  EXPECT_EQ(18, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Body.SizeInBytes"));
  EXPECT_EQ(28, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Icon.SizeInBytes"));
  EXPECT_EQ(29, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Image.SizeInBytes"));
  EXPECT_EQ(29, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Badge.SizeInBytes"));
  EXPECT_EQ(17, histogram_tester_.GetTotalSum(
                    "Notifications.Persistent.Data.SizeInBytes"));
  histogram_tester_.ExpectTotalCount(
      "Notifications.Persistent.Actions.Icon.SizeInBytes", 2);
  EXPECT_EQ(28 * 2, histogram_tester_.GetTotalSum(
                        "Notifications.Persistent.Actions.Icon.SizeInBytes"));
  EXPECT_EQ(8 * 2, histogram_tester_.GetTotalSum(
                       "Notifications.Persistent.Actions.Action.SizeInBytes"));
  EXPECT_EQ(7 * 2, histogram_tester_.GetTotalSum(
                       "Notifications.Persistent.Actions.Title.SizeInBytes"));
  EXPECT_EQ(13 * 2,
            histogram_tester_.GetTotalSum(
                "Notifications.Persistent.Actions.Placeholder.SizeInBytes"));
}
