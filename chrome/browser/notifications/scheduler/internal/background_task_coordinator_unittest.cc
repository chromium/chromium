// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/background_task_coordinator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace notifications {
namespace {

using Notifications = BackgroundTaskCoordinator::Notifications;
using ClientStates = BackgroundTaskCoordinator::ClientStates;

const char kNow[] = "04/25/1984 06:00:00 AM";
const char kDeliverTimeWindowStart[] = "04/25/1984 08:00:00 AM";
const char kDeliverTimeWindowEnd[] = "04/25/1984 08:50:00 AM";
const char kTommorow[] = "04/26/1984 00:00:00 AM";

const char kGuid[] = "1234";
const std::vector<test::ImpressionTestData> kSingleClientImpressionTestData = {{
    SchedulerClientType::kTest1,
    1 /* current_max_daily_show */,
    {} /* impressions */,
    std::nullopt /* suppression_info */,
    0 /* negative_events_count */,
    std::nullopt /* negative_event_ts */,
    std::nullopt /* last_shown_ts */
}

};

const std::vector<test::ImpressionTestData> kClientsImpressionTestData = {
    {SchedulerClientType::kTest1,
     1 /* current_max_daily_show */,
     {} /* impressions */,
     std::nullopt /* suppression_info */,
     0 /* negative_events_count */,
     std::nullopt /* negative_event_ts */,
     std::nullopt /* last_shown_ts */},
    {
        SchedulerClientType::kTest2,
        2 /* current_max_daily_show */,
        {} /* impressions */,
        std::nullopt /* suppression_info */,
        0 /* negative_events_count */,
        std::nullopt /* negative_event_ts */,
        std::nullopt /* last_shown_ts */,

    }};

struct TestData {
  // Impression data as the input.
  std::vector<test::ImpressionTestData> impression_test_data;

  // Notification entries as the input.
  std::vector<NotificationEntry> notification_entries;
};

class BackgroundTaskCoordinatorTest : public testing::Test {
 public:
  BackgroundTaskCoordinatorTest() = default;
  BackgroundTaskCoordinatorTest(const BackgroundTaskCoordinatorTest&) = delete;
  BackgroundTaskCoordinatorTest& operator=(
      const BackgroundTaskCoordinatorTest&) = delete;
  ~BackgroundTaskCoordinatorTest() override = default;

 protected:
  void SetUp() override {
    // Setup configuration used by this test.
    config_.max_daily_shown_all_type = 3;
    config_.max_daily_shown_per_type = 2;
    config_.suppression_duration = base::Days(3);

    auto background_task =
        std::make_unique<test::MockNotificationBackgroundTaskScheduler>();
    background_task_ = background_task.get();
    coordinator_ = BackgroundTaskCoordinator::Create(std::move(background_task),
                                                     &config_, &clock_);
    clock_.SetNow(kNow);
  }

  test::MockNotificationBackgroundTaskScheduler* background_task() {
    return background_task_;
  }
  SchedulerConfig* config() { return &config_; }
  test::FakeClock* clock() { return &clock_; }

  base::Time GetTime(const char* time_str) {
    return test::FakeClock::GetTime(time_str);
  }

  NotificationEntry CreateNotification(SchedulerClientType type,
                                       const std::string& guid,
                                       const char* deliver_window_start,
                                       const char* deliver_window_end) {
    NotificationEntry entry(type, guid);
    entry.schedule_params.deliver_time_start = GetTime(deliver_window_start);
    entry.schedule_params.deliver_time_end = GetTime(deliver_window_end);
    return entry;
  }

  void ScheduleTask(const TestData& test_data) {
    test_data_ = test_data;
    test::AddImpressionTestData(test_data_.impression_test_data,
                                &client_states_);
    std::map<SchedulerClientType, const ClientState*> client_states;
    for (const auto& type : client_states_) {
      client_states.emplace(type.first, type.second.get());
    }

    Notifications notifications;
    for (const auto& entry : test_data_.notification_entries) {
      notifications[entry.type].emplace_back(&entry);
    }
    coordinator_->ScheduleBackgroundTask(std::move(notifications),
                                         std::move(client_states));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::FakeClock clock_;
  SchedulerConfig config_;
  std::unique_ptr<BackgroundTaskCoordinator> coordinator_;
  raw_ptr<test::MockNotificationBackgroundTaskScheduler> background_task_;
  TestData test_data_;
  std::map<SchedulerClientType, std::unique_ptr<ClientState>> client_states_;
};

// No notification persisted, then no background task needs to be scheduled.
// And current task should be canceled.
TEST_F(BackgroundTaskCoordinatorTest, NoNotification) {
  EXPECT_CALL(*background_task(), Cancel());
  EXPECT_CALL(*background_task(), Schedule(_, _)).Times(0);
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  ScheduleTask(test_data);
}

// Test to schedule one notification.
TEST_F(BackgroundTaskCoordinatorTest, OneNotification) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         kDeliverTimeWindowStart, kDeliverTimeWindowEnd)};
  EXPECT_CALL(*background_task(),
              Schedule(GetTime(kDeliverTimeWindowStart) - GetTime(kNow), _));
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verifies that the daily throttle for a particular notification type will
// block notification to show.
TEST_F(BackgroundTaskCoordinatorTest, ThrottlePerType) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.impression_test_data.front().current_max_daily_show = 0;
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         kDeliverTimeWindowStart, kDeliverTimeWindowEnd)};
  EXPECT_CALL(*background_task(), Schedule(_, _)).Times(0);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verifies that the daily throttle for all notification types will
// block notification to show.
TEST_F(BackgroundTaskCoordinatorTest, ThrottleAllType) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.impression_test_data.front().current_max_daily_show = 1;
  config()->max_daily_shown_all_type = 0;
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         kDeliverTimeWindowStart, kDeliverTimeWindowEnd)};
  EXPECT_CALL(*background_task(), Schedule(_, _)).Times(0);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verifies that a notification scheduled to show after today will still trigger
// a background task even if it is throttled today.
TEST_F(BackgroundTaskCoordinatorTest, ThrottlePerTypeNextDay) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.impression_test_data.front().current_max_daily_show = 1;
  Impression impression_today(SchedulerClientType::kTest1, "guid",
                              clock()->Now() - base::Minutes(5));
  test_data.impression_test_data.front().impressions = {impression_today};
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         "04/25/1984 23:59:00 PM", "04/26/1984 08:00:00 AM")};
  EXPECT_CALL(*background_task(),
              Schedule(GetTime(kTommorow) - GetTime(kNow), _));
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verfies that notification with their deliver window expired will not trigger
// a background task.
TEST_F(BackgroundTaskCoordinatorTest, DeliverWindowPassed) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         "04/24/1984 23:59:00 PM", "04/24/1984 08:00:00 AM")};
  EXPECT_CALL(*background_task(), Schedule(_, _)).Times(0);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verfies that notification suppression will block the notification to be
// shown.
TEST_F(BackgroundTaskCoordinatorTest, Suppression) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         kDeliverTimeWindowStart, kDeliverTimeWindowEnd)};
  test_data.impression_test_data.front().suppression_info =
      SuppressionInfo(clock()->Now() - base::Hours(1), base::Days(7));
  EXPECT_CALL(*background_task(), Schedule(_, _)).Times(0);
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verfies that notification will trigger background task if its deliver time
// window is after the suppression expiration time.
TEST_F(BackgroundTaskCoordinatorTest, DeliverTimeAfterSuppressionExpired) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.notification_entries = {
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         "04/26/1984 05:00:00 AM", "04/26/1984 23:59:00 PM")};
  // Suppression will expire at 04/26/1984 06:00:00 AM.
  test_data.impression_test_data.front().suppression_info =
      SuppressionInfo(clock()->Now() - base::Days(1), base::Days(2));
  EXPECT_CALL(*background_task(),
              Schedule(GetTime("04/26/1984 06:00:00 AM") - GetTime(kNow), _));
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Test to schedule multiple notifications from multiple clients.
TEST_F(BackgroundTaskCoordinatorTest, MutipleNotifications) {
  TestData test_data;
  test_data.impression_test_data = kClientsImpressionTestData;
  NotificationEntry entry0 =
      CreateNotification(SchedulerClientType::kTest1, "guid0",
                         kDeliverTimeWindowStart, kDeliverTimeWindowEnd);
  NotificationEntry entry1 =
      CreateNotification(SchedulerClientType::kTest2, "guid1",
                         "04/27/1984 05:00:00 AM", "04/27/1984 23:59:00 PM");

  test_data.notification_entries = {entry0, entry1};
  EXPECT_CALL(*background_task(),
              Schedule(GetTime(kDeliverTimeWindowStart) - GetTime(kNow), _));
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

// Verifies that the notification with NoThrottle priority will always trigger
// background task.
TEST_F(BackgroundTaskCoordinatorTest, NoThrottleNotifications) {
  TestData test_data;
  test_data.impression_test_data = kSingleClientImpressionTestData;
  test_data.impression_test_data.front().current_max_daily_show = 1;
  config()->max_daily_shown_all_type = 0;
  auto entry =
      CreateNotification(SchedulerClientType::kTest1, kGuid,
                         kDeliverTimeWindowStart, kDeliverTimeWindowEnd);
  entry.schedule_params.priority = ScheduleParams::Priority::kNoThrottle;
  test_data.notification_entries = {entry};
  EXPECT_CALL(*background_task(),
              Schedule(GetTime(kDeliverTimeWindowStart) - GetTime(kNow), _));
  EXPECT_CALL(*background_task(), Cancel()).Times(0);
  ScheduleTask(test_data);
}

}  // namespace
}  // namespace notifications
