// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/display_decider.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/notification_entry.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

// Initial state for test cases with a single registered client.
const std::vector<test::ImpressionTestData> kSingleClientImpressionTestData = {
    {SchedulerClientType::kTest1,
     2 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

const std::vector<test::ImpressionTestData> kClientsImpressionTestData = {
    {SchedulerClientType::kTest1,
     2 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */},
    {SchedulerClientType::kTest2,
     5 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */},
    {SchedulerClientType::kTest3,
     1 /* current_max_daily_show */,
     {},
     base::nullopt /* suppression_info */}};

struct TestData {
  // Impression data as the input.
  std::vector<test::ImpressionTestData> impression_test_data;

  // Notification entries as the input.
  std::vector<NotificationEntry> notification_entries;

  // Expected output data.
  DisplayDecider::Results expected;
};

std::string DebugString(const DisplayDecider::Results& results) {
  std::string debug_string("notifications_to_show: \n");
  for (const auto& guid : results)
    debug_string += base::StringPrintf("%s ", guid.c_str());

  return debug_string;
}

class DisplayDeciderTest : public testing::Test {
 public:
  DisplayDeciderTest() = default;
  ~DisplayDeciderTest() override = default;

  void SetUp() override {
    // Setup configuration used by this test.
    config_.max_daily_shown_all_type = 3;

    // Fake Now() timestamp.
    base::Time fake_now;
    ASSERT_TRUE(base::Time::FromString("04/25/20 01:00:00 AM", &fake_now));
    clock_.SetNow(fake_now);
  }

 protected:
  // Initializes a test case with input data.
  void RunTestCase(const TestData& test_data) {
    test_data_ = test_data;
    test::AddImpressionTestData(test_data_.impression_test_data,
                                &client_states_);

    DisplayDecider::Notifications notifications;
    for (const auto& entry : test_data_.notification_entries) {
      notifications[entry.type].emplace_back(&entry);
    }
    std::vector<SchedulerClientType> clients;

    std::map<SchedulerClientType, const ClientState*> client_states;
    for (const auto& type : client_states_) {
      client_states.emplace(type.first, type.second.get());
      clients.emplace_back(type.first);
    }

    // Copy test inputs into |decider_|.
    decider_ = DisplayDecider::Create(&config_, clients, &clock_);
    decider_->FindNotificationsToShow(std::move(notifications),
                                      std::move(client_states), &results_);

    // Verify output.
    EXPECT_EQ(results_, test_data_.expected)
        << "Actual result: \n"
        << DebugString(results_) << " \n Expected result: \n"
        << DebugString(test_data_.expected);
  }

  // Creates a notification entry. Now() timestamp falls into its deliver time
  // window.
  NotificationEntry CreateNotification(SchedulerClientType type,
                                       const std::string& guid) {
    return CreateNotification(type, guid, base::TimeDelta(),
                              base::TimeDelta::FromHours(1));
  }

  // Creates a notification entry with specific deliver time window.
  NotificationEntry CreateNotification(
      SchedulerClientType type,
      const std::string& guid,
      base::Optional<base::TimeDelta> deliver_time_start_delta,
      base::Optional<base::TimeDelta> deliver_time_end_delta) {
    NotificationEntry entry(type, guid);
    if (deliver_time_start_delta.has_value())
      entry.schedule_params.deliver_time_start =
          Now() + deliver_time_start_delta.value();
    if (deliver_time_end_delta.has_value())
      entry.schedule_params.deliver_time_end =
          Now() + deliver_time_end_delta.value();

    return entry;
  }

  SchedulerConfig* config() { return &config_; }

  base::Time Now() { return clock_.Now(); }

 private:
  base::test::TaskEnvironment task_environment_;

  TestData test_data_;
  SchedulerConfig config_;
  test::FakeClock clock_;

  std::map<SchedulerClientType, std::unique_ptr<ClientState>> client_states_;

  // Test target class and output.
  std::unique_ptr<DisplayDecider> decider_;
  DisplayDecider::Results results_;

  DISALLOW_COPY_AND_ASSIGN(DisplayDeciderTest);
};

TEST_F(DisplayDeciderTest, NoNotification) {
  TestData data{kClientsImpressionTestData, {}, DisplayDecider::Results()};
  RunTestCase(data);
}

// Simple test case to verify new notifcaiton can be selected to show.
TEST_F(DisplayDeciderTest, PickOneNotification) {
  auto entry = CreateNotification(SchedulerClientType::kTest1, "guid123");
  DisplayDecider::Results expected = {"guid123"};

  TestData data{kSingleClientImpressionTestData, {entry}, std::move(expected)};
  RunTestCase(data);
}

// Notification falls out of the target deliver time window will not be picked.
TEST_F(DisplayDeciderTest, OutOfDeliverTimeWindow) {
  auto entry0 = CreateNotification(SchedulerClientType::kTest2, "guid0",
                                   base::TimeDelta::FromDays(1),
                                   base::TimeDelta::FromDays(2));
  auto entry1 =
      CreateNotification(SchedulerClientType::kTest2, "guid1",
                         base::TimeDelta() - base::TimeDelta::FromDays(2),
                         base::TimeDelta() - base::TimeDelta::FromDays(1));
  auto entry2 = CreateNotification(SchedulerClientType::kTest2, "guid2",
                                   base::nullopt, base::nullopt);

  TestData data{kSingleClientImpressionTestData,
                {entry0, entry1, entry2},
                DisplayDecider::Results()};
  RunTestCase(data);
}

// Picks a notification for the next client if possible.
// Rotation client type sequence:  kTest3 => kTest2 => kTest1
// The last shown type is kTest1. Expected to show for client kTest3.
TEST_F(DisplayDeciderTest, ClientRotation) {
  config()->max_daily_shown_all_type = 2;
  auto impression_test_data = kClientsImpressionTestData;

  auto entry1 = CreateNotification(SchedulerClientType::kTest1, "guid1");

  // Create an impression shown today.
  Impression impression(SchedulerClientType::kTest1, "shown_guid1",
                        Now() - base::TimeDelta::FromHours(1));
  impression_test_data.front().impressions.emplace_back(impression);

  auto entry2 = CreateNotification(SchedulerClientType::kTest2, "guid2");
  auto entry3 = CreateNotification(SchedulerClientType::kTest3, "guid3");

  TestData data{impression_test_data, {entry1, entry2, entry3}, {"guid3"}};
  RunTestCase(data);
}

// After reaching maximum daily shown throttle, no notifications will be picked.
TEST_F(DisplayDeciderTest, ThrottleMaxDailyShowAllTypes) {
  auto impression_test_data = kClientsImpressionTestData;

  auto entry1 = CreateNotification(SchedulerClientType::kTest1, "guid1");

  // Create an impression shown today, but only allow to show one per day for
  // all clients.
  Impression impression(SchedulerClientType::kTest1, "shown_guid1",
                        Now() - base::TimeDelta::FromHours(1));
  impression_test_data.front().impressions.emplace_back(impression);
  config()->max_daily_shown_all_type = 1;

  TestData data{impression_test_data, {entry1}, DisplayDecider::Results()};
  RunTestCase(data);
}

// After reaching client's maximum daily shown throttle, notifications scheduled
// for this client will not be show.
TEST_F(DisplayDeciderTest, ThrottlePerClient) {
  config()->max_daily_shown_all_type = 10;
  auto impression_test_data = kClientsImpressionTestData;

  // Have 2 notifications, but only one can be shown due to per client throttle.
  impression_test_data.front().current_max_daily_show = 2;
  auto entry1 = CreateNotification(SchedulerClientType::kTest1, "guid1");
  auto entry2 = CreateNotification(SchedulerClientType::kTest1, "guid2");

  // Create an impression shown today.
  Impression impression(SchedulerClientType::kTest1, "shown_guid1",
                        Now() - base::TimeDelta::FromHours(1));
  impression_test_data.front().impressions.emplace_back(impression);

  TestData data{impression_test_data, {entry1, entry2}, {"guid2"}};
  RunTestCase(data);
}

// Client with suppression will not have notification shown.
TEST_F(DisplayDeciderTest, ThrottleSuppressedClient) {
  auto impression_test_data = kClientsImpressionTestData;
  impression_test_data.front().suppression_info =
      SuppressionInfo(Now(), base::TimeDelta::FromDays(10));
  auto entry1 = CreateNotification(SchedulerClientType::kTest1, "guid1");

  TestData data{impression_test_data, {entry1}, DisplayDecider::Results()};
  RunTestCase(data);
}

// Notifitions with NoThrottle Priority should always show.
TEST_F(DisplayDeciderTest, UnthrottlePriority) {
  auto impression_test_data = kClientsImpressionTestData;
  auto entry1 = CreateNotification(SchedulerClientType::kTest1, "guid1");
  entry1.schedule_params.priority = ScheduleParams::Priority::kNoThrottle;
  auto entry2 = CreateNotification(SchedulerClientType::kTest1, "guid2");
  entry2.schedule_params.priority = ScheduleParams::Priority::kNoThrottle;
  config()->max_daily_shown_all_type = 0;
  TestData data{impression_test_data, {entry1, entry2}, {"guid1", "guid2"}};
  RunTestCase(data);
}

}  // namespace
}  // namespace notifications
