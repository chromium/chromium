// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/clients/finds_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/finds/core/finds_metrics.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/notifications/scheduler/public/finds_agent.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace notifications {
namespace {

class MockFindsAgent : public FindsAgent {
 public:
  MOCK_METHOD(void, OpenNotificationUrl, (const GURL& url), (override));
};

}  // namespace

class FindsClientTest : public testing::Test {
 public:
  FindsClientTest() = default;

  void SetUp() override {
    finds::FindsService::RegisterProfilePrefs(pref_service_.registry());
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        pref_service_.registry());
    auto mock_finds_agent = std::make_unique<MockFindsAgent>();
    mock_finds_agent_ = mock_finds_agent.get();
    finds_client_ = std::make_unique<FindsClient>(std::move(mock_finds_agent),
                                                  &pref_service_);
  }

 protected:
  NotificationSchedulerClient* finds_client() { return finds_client_.get(); }
  MockFindsAgent* mock_finds_agent() { return mock_finds_agent_; }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  std::unique_ptr<FindsClient> finds_client_;
  raw_ptr<MockFindsAgent> mock_finds_agent_;
  TestingPrefServiceSimple pref_service_;
  base::HistogramTester histogram_tester_;
};

// Verifies that a click action calls the FindsAgent OpenNotificationUrl
// function.
TEST_F(FindsClientTest, OnUserAction_Click) {
  UserActionData action_data(SchedulerClientType::kChromeFinds,
                             UserActionType::kClick, "guid1");
  const char kTestUrl[] = "https://www.google.com/";
  action_data.custom_data[kChromeFindsNotificationsUrl] = kTestUrl;

  EXPECT_CALL(*mock_finds_agent(), OpenNotificationUrl(GURL(kTestUrl)));
  finds_client()->OnUserAction(action_data);

  histogram_tester()->ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationInteraction",
      finds::FindsNotificationUserInteraction::kClick, 1);
}

// Verifies that a helpful button click action calls the FindsAgent
// OpenNotificationUrl function.
TEST_F(FindsClientTest, OnUserAction_HelpfulButtonClick) {
  UserActionData action_data(SchedulerClientType::kChromeFinds,
                             UserActionType::kButtonClick, "guid1");
  const char kTestUrl[] = "https://www.google.com/";
  action_data.custom_data[kChromeFindsNotificationsUrl] = kTestUrl;
  ButtonClickInfo button_info;
  button_info.type = ActionButtonType::kHelpful;
  action_data.button_click_info = button_info;

  EXPECT_CALL(*mock_finds_agent(), OpenNotificationUrl(GURL(kTestUrl)));
  finds_client()->OnUserAction(action_data);

  histogram_tester()->ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationInteraction",
      finds::FindsNotificationUserInteraction::kHelpfulButtonClick, 1);
}

// Verifies that an unhelpful button click action marks the theme as not
// interested.
TEST_F(FindsClientTest, OnUserAction_UnhelpfulButtonClick) {
  UserActionData action_data(SchedulerClientType::kChromeFinds,
                             UserActionType::kButtonClick, "guid1");
  optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme::ThemeType
      theme_type = optimization_guide::proto::FindsSuggestionResponse::
          SuggestionTheme::SHOPPING;
  action_data.custom_data[kChromeFindsNotificationsThemeType] =
      base::NumberToString(static_cast<int>(theme_type));
  ButtonClickInfo button_info;
  button_info.type = ActionButtonType::kUnhelpful;
  action_data.button_click_info = button_info;

  finds_client()->OnUserAction(action_data);

  const base::DictValue& not_interested_themes = pref_service()->GetDict(
      finds::prefs::kFindsNotInterestedThemesLastTimestamp);
  EXPECT_TRUE(not_interested_themes.contains("Shopping"));

  histogram_tester()->ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationInteraction",
      finds::FindsNotificationUserInteraction::kUnhelpfulButtonClick, 1);
}

TEST_F(FindsClientTest, OnUserAction_Dismiss) {
  UserActionData action_data(SchedulerClientType::kChromeFinds,
                             UserActionType::kDismiss, "guid1");

  finds_client()->OnUserAction(action_data);

  histogram_tester()->ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationInteraction",
      finds::FindsNotificationUserInteraction::kDismiss, 1);
}

// Verifies that OnShowNotification records metrics.
TEST_F(FindsClientTest, OnShowNotification) {
  finds_client()->OnShowNotification(nullptr);

  histogram_tester()->ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationShown", true, 1);
}

TEST_F(FindsClientTest, BeforeShowNotification_EnterprisePolicyAllowed) {
  pref_service()->SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kAllow));

  auto notification_data = std::make_unique<NotificationData>();
  notification_data->title = u"Test Title";

  base::test::TestFuture<std::unique_ptr<NotificationData>> future;
  finds_client()->BeforeShowNotification(std::move(notification_data),
                                         future.GetCallback());

  std::unique_ptr<NotificationData> returned_data = future.Take();
  EXPECT_NE(nullptr, returned_data);
  EXPECT_EQ(u"Test Title", returned_data->title);
}

TEST_F(FindsClientTest, BeforeShowNotification_EnterprisePolicyDisabled) {
  pref_service()->SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));

  auto notification_data = std::make_unique<NotificationData>();
  notification_data->title = u"Test Title";

  base::test::TestFuture<std::unique_ptr<NotificationData>> future;
  finds_client()->BeforeShowNotification(std::move(notification_data),
                                         future.GetCallback());

  EXPECT_EQ(nullptr, future.Get());
}

}  // namespace notifications
