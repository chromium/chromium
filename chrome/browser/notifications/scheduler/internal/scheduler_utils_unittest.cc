// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/scheduler_utils.h"

#include <algorithm>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/guid.h"
#include "base/test/task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/impression_types.h"
#include "chrome/browser/notifications/scheduler/test/fake_clock.h"
#include "chrome/browser/notifications/scheduler/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

const char kFakeNow[] = "01/01/18 01:23:45 AM";

class SchedulerUtilsTest : public testing::Test {
 public:
  SchedulerUtilsTest() {}
  ~SchedulerUtilsTest() override = default;

  void SetUp() override { config_.initial_daily_shown_per_type = 100; }

  void InitFakeClock() {
    clock_.SetNow(kFakeNow);
    ToLocalHour(0, clock_.Now(), 0, &beginning_of_today_);
  }

  void CreateFakeImpressions(ClientState* client_state,
                             const std::vector<base::Time>& times) {
    DCHECK(client_state);
    client_state->impressions.clear();
    auto type = client_state->type;
    for (const auto& time : times) {
      client_state->impressions.emplace_back(type, base::GenerateGUID(), time);
    }
  }

  std::unique_ptr<ClientState> CreateFakeClientStateWithImpression(
      SchedulerClientType type,
      const SchedulerConfig& config,
      const std::vector<base::Time>& times) {
    auto client_state = std::make_unique<ClientState>();
    client_state->type = type;
    client_state->current_max_daily_show = config.initial_daily_shown_per_type;
    CreateFakeImpressions(client_state.get(), times);
    return client_state;
  }

 protected:
  SchedulerConfig& config() { return config_; }
  test::FakeClock* clock() { return &clock_; }
  base::Time& beginning_of_today() { return beginning_of_today_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::FakeClock clock_;
  SchedulerConfig config_;
  base::Time beginning_of_today_;
  DISALLOW_COPY_AND_ASSIGN(SchedulerUtilsTest);
};

// Verifies we can get the correct time stamp at certain hour in yesterday.
TEST_F(SchedulerUtilsTest, ToLocalHour) {
  base::Time today, another_day, expected;

  // Timestamp of another day in the past.
  EXPECT_TRUE(base::Time::FromString("10/15/07 12:45:12 PM", &today));
  EXPECT_TRUE(ToLocalHour(6, today, -1, &another_day));
  EXPECT_TRUE(base::Time::FromString("10/14/07 06:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalHour(0, today, -1, &another_day));
  EXPECT_TRUE(base::Time::FromString("03/24/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  // Timestamp of the same day.
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalHour(0, today, 0, &another_day));
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  // Timestamp of another day in the future.
  EXPECT_TRUE(base::Time::FromString("03/25/19 06:35:27 AM", &today));
  EXPECT_TRUE(ToLocalHour(16, today, 7, &another_day));
  EXPECT_TRUE(base::Time::FromString("04/01/19 16:00:00 PM", &expected));
  EXPECT_EQ(expected, another_day);
}

TEST_F(SchedulerUtilsTest, NotificationsShownTodayMultipleClients) {
  InitFakeClock();
  base::Time now = clock()->Now();
  // Create fake clients.
  std::map<SchedulerClientType, const ClientState*> client_states;
  //            begin_of_today         now                  end_of_today
  // client1  * |                   *  *                    | *
  // client2  * *   *                  |  *                 | *
  // client3  * |                      |                    | *

  std::vector<base::Time> create_times = {
      now - base::TimeDelta::FromSeconds(2) /*today*/,
      now - base::TimeDelta::FromSeconds(1) /*today*/,
      beginning_of_today() - base::TimeDelta::FromSeconds(1) /*yesterday*/,
      beginning_of_today() + base::TimeDelta::FromDays(1) /*tomorrow*/};
  auto new_client1 = CreateFakeClientStateWithImpression(
      SchedulerClientType::kTest1, config(), create_times);

  create_times = {
      now /*today*/,
      beginning_of_today() + base::TimeDelta::FromDays(1) /*tomorrow*/,
      beginning_of_today() - base::TimeDelta::FromSeconds(1) /*yesterday*/,
      beginning_of_today() + base::TimeDelta::FromSeconds(1) /*today*/,
      beginning_of_today() /*today*/};
  auto new_client2 = CreateFakeClientStateWithImpression(
      SchedulerClientType::kTest2, config(), create_times);

  create_times = {
      beginning_of_today() - base::TimeDelta::FromSeconds(2), /*yesterday*/
      beginning_of_today() + base::TimeDelta::FromDays(1)     /*tomorrow*/
  };
  auto new_client3 = CreateFakeClientStateWithImpression(
      SchedulerClientType::kTest3, config(), create_times);

  client_states[SchedulerClientType::kTest1] = new_client1.get();
  client_states[SchedulerClientType::kTest2] = new_client2.get();
  client_states[SchedulerClientType::kTest3] = new_client3.get();

  std::map<SchedulerClientType, int> shown_per_type;
  int shown_total = 0;
  SchedulerClientType last_shown_type = SchedulerClientType::kUnknown;
  NotificationsShownToday(client_states, &shown_per_type, &shown_total,
                          &last_shown_type, clock());
  EXPECT_EQ(shown_total, 5);
  EXPECT_EQ(last_shown_type, SchedulerClientType::kTest2);
  EXPECT_EQ(shown_per_type.at(SchedulerClientType::kTest1), 2);
  EXPECT_EQ(shown_per_type.at(SchedulerClientType::kTest2), 3);
  EXPECT_EQ(shown_per_type.at(SchedulerClientType::kTest3), 0);
}

TEST_F(SchedulerUtilsTest, NotificationsShownToday) {
  // Create fake client.
  auto new_client = CreateNewClientState(SchedulerClientType::kTest1, config());
  InitFakeClock();
  base::Time now = clock()->Now();
  // Test case 1:
  int count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 0);

  // Test case 2:
  std::vector<base::Time> create_times = {
      now /*today*/,
      beginning_of_today() + base::TimeDelta::FromDays(1) /*tomorrow*/,
      beginning_of_today() - base::TimeDelta::FromSeconds(1) /*yesterday*/,
      beginning_of_today() + base::TimeDelta::FromSeconds(1) /*today*/,
      beginning_of_today() /*today*/};

  CreateFakeImpressions(new_client.get(), create_times);
  count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 3);

  // Test case 3:
  create_times = {
      beginning_of_today() - base::TimeDelta::FromSeconds(2), /*yesterday*/
      beginning_of_today() + base::TimeDelta::FromDays(1),    /*tomorrow*/
  };
  CreateFakeImpressions(new_client.get(), create_times);
  count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 0);

  // Test case 4:
  create_times = {
      now /*today*/, now - base::TimeDelta::FromSeconds(1) /*today*/,
      beginning_of_today() - base::TimeDelta::FromSeconds(1) /*yesterday*/,
      beginning_of_today() + base::TimeDelta::FromDays(1) /*tomorrow*/};
  CreateFakeImpressions(new_client.get(), create_times);
  count = NotificationsShownToday(new_client.get(), clock());
  EXPECT_EQ(count, 2);
}

}  // namespace
}  // namespace notifications
