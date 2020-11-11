// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/public/throttle_config.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/offline_pages/prefetch/notifications/prefetch_notification_service_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace prefetch {
namespace {

using testing::_;
using ::testing::Invoke;
using MockScheduler = notifications::test::MockNotificationScheduleService;

const auto kTestTitle = base::UTF8ToUTF16("hello");
const auto kTestMessage = base::UTF8ToUTF16("world");

class MockBridge : public PrefetchNotificationServiceBridge {
 public:
  MockBridge() = default;
  ~MockBridge() override = default;
  MOCK_METHOD(void, LaunchDownloadHome, ());
};

class PrefetchNotificationServiceImplTest : public testing::Test {
 public:
  PrefetchNotificationServiceImplTest() {}
  ~PrefetchNotificationServiceImplTest() override = default;
  PrefetchNotificationServiceImplTest(
      const PrefetchNotificationServiceImplTest& other) = delete;
  PrefetchNotificationServiceImplTest& operator=(
      const PrefetchNotificationServiceImplTest& other) = delete;

  void SetUp() override {
    scheduler_ = std::make_unique<MockScheduler>();
    auto bridge = std::make_unique<MockBridge>();
    bridge_ = bridge.get();
    service_ = std::make_unique<PrefetchNotificationServiceImpl>(
        scheduler_.get(), std::move(bridge), &clock_);
  }

 protected:
  MockScheduler* scheduler() { return scheduler_.get(); }
  MockBridge* bridge() { return bridge_; }
  PrefetchNotificationService* service() { return service_.get(); }
  base::SimpleTestClock* clock() { return &clock_; }

 private:
  base::SimpleTestClock clock_;
  base::test::TaskEnvironment task_environment_;
  MockBridge* bridge_;
  std::unique_ptr<MockScheduler> scheduler_;
  std::unique_ptr<PrefetchNotificationService> service_;
};

MATCHER_P(NotificationParamsEq,
          expected,
          "Compare the notification params except GUID") {
  EXPECT_EQ(arg->schedule_params, expected->schedule_params);
  EXPECT_EQ(arg->notification_data, expected->notification_data);
  EXPECT_EQ(arg->enable_ihnr_buttons, expected->enable_ihnr_buttons);
  EXPECT_EQ(arg->type, expected->type);
  return true;
}

TEST_F(PrefetchNotificationServiceImplTest, ScheduleSuccess) {
  // Build expected NotificationParams..
  notifications::NotificationData data;
  data.title = kTestTitle;
  data.message = kTestMessage;
  notifications::ScheduleParams schedule_params;
  schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kDismiss,
      notifications::ImpressionResult::kNegative);
  schedule_params.impression_mapping.emplace(
      notifications::UserFeedback::kIgnore,
      notifications::ImpressionResult::kNegative);
  base::Time fake_now;
  EXPECT_TRUE(base::Time::FromString("05/18/20 01:00:00 AM", &fake_now));
  schedule_params.deliver_time_start = base::make_optional(fake_now);
  schedule_params.deliver_time_end =
      base::make_optional(fake_now + base::TimeDelta::FromMinutes(1));
  schedule_params.ignore_timeout_duration = base::TimeDelta::FromDays(1);
  notifications::NotificationParams expected_params(
      notifications::SchedulerClientType::kPrefetch, std::move(data),
      std::move(schedule_params));
  expected_params.enable_ihnr_buttons = true;

  clock()->SetNow(fake_now);

  EXPECT_CALL(
      *scheduler(),
      GetClientOverview(notifications::SchedulerClientType::kPrefetch, _))
      .WillOnce(Invoke(
          [](notifications::SchedulerClientType client,
             base::OnceCallback<void(notifications::ClientOverview)> callback) {
            notifications::ClientOverview client_overview;
            client_overview.impression_detail.current_max_daily_show = 1;
            std::move(callback).Run(std::move(client_overview));
          }));
  EXPECT_CALL(*scheduler(), Schedule(NotificationParamsEq(&expected_params)));

  service()->Schedule(kTestTitle, kTestMessage);
}

TEST_F(PrefetchNotificationServiceImplTest, NoScheduleUnderThrottle) {
  EXPECT_CALL(
      *scheduler(),
      GetClientOverview(notifications::SchedulerClientType::kPrefetch, _))
      .WillOnce(Invoke(
          [](notifications::SchedulerClientType client,
             base::OnceCallback<void(notifications::ClientOverview)> callback) {
            notifications::ClientOverview client_overview;
            // Scheduler tells this notification is currently under throttle.
            client_overview.impression_detail.current_max_daily_show = 0;
            std::move(callback).Run(std::move(client_overview));
          }));
  EXPECT_CALL(*scheduler(), Schedule(_)).Times(0);
  service()->Schedule(kTestTitle, kTestMessage);
}

TEST_F(PrefetchNotificationServiceImplTest, OnClick) {
  EXPECT_CALL(*bridge(), LaunchDownloadHome());
  service()->OnClick();
}

// Verify throttle config in first stage: suspend for 7 days after 1 negative
// event(3 consecutive dismiss).
TEST_F(PrefetchNotificationServiceImplTest, GetThrottleConfigFirstStage) {
  EXPECT_CALL(
      *scheduler(),
      GetClientOverview(notifications::SchedulerClientType::kPrefetch, _))
      .WillOnce(Invoke(
          [](notifications::SchedulerClientType client,
             base::OnceCallback<void(notifications::ClientOverview)> callback) {
            notifications::ClientOverview client_overview;
            // First stage: no throttle happened yet.
            client_overview.impression_detail.num_negative_events = 0;
            std::move(callback).Run(std::move(client_overview));
          }));
  service()->GetThrottleConfig(base::BindOnce(
      [](std::unique_ptr<notifications::ThrottleConfig> throttle_config) {
        EXPECT_EQ(throttle_config->suppression_duration.value(),
                  base::TimeDelta::FromDays(7));
        // Using default value kDismissCountConfig(3) in SchedulerConfig.
        EXPECT_FALSE(
            throttle_config->negative_action_count_threshold.has_value());
      }));
}

// Verify throttle config in second stage: suspend for 7 days after 1 negative
// event(only 1 dismiss will cause it -
// throttle_config->negative_action_count_threshold == 1).
TEST_F(PrefetchNotificationServiceImplTest, GetThrottleConfigSecondStage) {
  EXPECT_CALL(
      *scheduler(),
      GetClientOverview(notifications::SchedulerClientType::kPrefetch, _))
      .WillOnce(Invoke(
          [](notifications::SchedulerClientType client,
             base::OnceCallback<void(notifications::ClientOverview)> callback) {
            notifications::ClientOverview client_overview;
            // Second stage: throttle has happened already.
            client_overview.impression_detail.num_negative_events = 1;
            std::move(callback).Run(std::move(client_overview));
          }));
  service()->GetThrottleConfig(base::BindOnce(
      [](std::unique_ptr<notifications::ThrottleConfig> throttle_config) {
        EXPECT_EQ(throttle_config->suppression_duration.value(),
                  base::TimeDelta::FromDays(7));
        EXPECT_EQ(throttle_config->negative_action_count_threshold.value(), 1);
      }));
}

}  // namespace
}  // namespace prefetch
}  // namespace offline_pages
