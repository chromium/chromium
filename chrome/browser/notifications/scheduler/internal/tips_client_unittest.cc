// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/tips_client.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/public/tips_agent.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

class MockTipsAgent : public TipsAgent {
 public:
  MOCK_METHOD(void,
              ShowTipsPromo,
              (TipsNotificationsFeatureType feature_type),
              (override));
};

}  // namespace

class TipsClientTest : public testing::Test {
 public:
  TipsClientTest() {
#if BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kAndroidTipNotificationShownESB, false);
#endif  // BUILDFLAG(IS_ANDROID)
  }

 public:
  void SetUp() override {
    auto mock_tips_agent = std::make_unique<MockTipsAgent>();
    mock_tips_agent_ = mock_tips_agent.get();
    tips_client_ = std::make_unique<TipsClient>(std::move(mock_tips_agent),
                                                &pref_service_);
  }

 protected:
  NotificationSchedulerClient* tips_client() { return tips_client_.get(); }
  MockTipsAgent* mock_tips_agent() { return mock_tips_agent_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  std::unique_ptr<TipsClient> tips_client_;
  raw_ptr<MockTipsAgent> mock_tips_agent_;
  TestingPrefServiceSimple pref_service_;
};

#if BUILDFLAG(IS_ANDROID)
// Verifies that a dismiss action is ignored.
TEST_F(TipsClientTest, OnUserActionDismiss) {
  base::HistogramTester histograms;
  UserActionData action_data(SchedulerClientType::kTips,
                             UserActionType::kDismiss, "guid1");
  action_data.custom_data[kTipsNotificationsFeatureType] = base::NumberToString(
      static_cast<int>(TipsNotificationsFeatureType::kEnhancedSafeBrowsing));
  EXPECT_CALL(*mock_tips_agent(), ShowTipsPromo(testing::_)).Times(0);
  tips_client()->OnUserAction(action_data);
  histograms.ExpectBucketCount("Notifications.Scheduler.Tips.FeatureTypeAction",
                               UserActionType::kDismiss, 1);
  histograms.ExpectBucketCount(
      "Notifications.Scheduler.Tips.FeatureTypeAction.EnhancedSafeBrowsing",
      UserActionType::kDismiss, 1);
}

// Verifies that a valid feature tip is shown.
TEST_F(TipsClientTest, OnUserActionShowFeatureTip) {
  base::HistogramTester histograms;
  UserActionData action_data(SchedulerClientType::kTips, UserActionType::kClick,
                             "guid1");
  action_data.custom_data[kTipsNotificationsFeatureType] = base::NumberToString(
      static_cast<int>(TipsNotificationsFeatureType::kEnhancedSafeBrowsing));
  EXPECT_CALL(
      *mock_tips_agent(),
      ShowTipsPromo(TipsNotificationsFeatureType::kEnhancedSafeBrowsing));
  tips_client()->OnUserAction(action_data);
  histograms.ExpectBucketCount("Notifications.Scheduler.Tips.FeatureTypeAction",
                               UserActionType::kClick, 1);
  histograms.ExpectBucketCount(
      "Notifications.Scheduler.Tips.FeatureTypeAction.EnhancedSafeBrowsing",
      UserActionType::kClick, 1);
}

// Verifies that the pref is set before showing the notification.
TEST_F(TipsClientTest, BeforeShowNotification) {
  base::HistogramTester histograms;
  auto notification_data = std::make_unique<NotificationData>();
  notification_data->custom_data[kTipsNotificationsFeatureType] =
      base::NumberToString(static_cast<int>(
          TipsNotificationsFeatureType::kEnhancedSafeBrowsing));
  EXPECT_FALSE(
      pref_service()->GetBoolean(prefs::kAndroidTipNotificationShownESB));
  tips_client()->BeforeShowNotification(
      std::move(notification_data),
      base::BindOnce([](std::unique_ptr<NotificationData> notification_data) {
        EXPECT_TRUE(notification_data);
      }));
  EXPECT_TRUE(
      pref_service()->GetBoolean(prefs::kAndroidTipNotificationShownESB));
  histograms.ExpectBucketCount(
      "Notifications.Scheduler.Tips.FeatureTypeShown",
      TipsNotificationsFeatureType::kEnhancedSafeBrowsing, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace notifications
