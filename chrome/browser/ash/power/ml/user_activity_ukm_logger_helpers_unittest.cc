// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_helpers.h"

#include <array>
#include <memory>

#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace ml {

TEST(UserActivityUkmLoggerBucketizerTest, BucketEveryFivePercents) {
  constexpr std::array<int, 4> original_values = {{0, 14, 15, 100}};
  constexpr std::array<int, 4> results = {{0, 10, 15, 100}};
  constexpr std::array<Bucket, 1> buckets = {{{100, 5}}};

  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(results[i], Bucketize(original_values[i], buckets));
  }
}

TEST(UserActivityUkmLoggerBucketizerTest, Bucketize) {
  constexpr std::array<int, 14> original_values = {
      {0, 18, 59, 60, 62, 69, 72, 299, 300, 306, 316, 599, 600, 602}};
  constexpr std::array<int, 14> results = {
      {0, 18, 59, 60, 60, 60, 70, 290, 300, 300, 300, 580, 600, 600}};
  constexpr std::array<Bucket, 3> buckets = {{{60, 1}, {300, 10}, {600, 20}}};
  for (size_t i = 0; i < original_values.size(); ++i) {
    EXPECT_EQ(results[i], Bucketize(original_values[i], buckets));
  }
}

TEST(UserActivityUkmLoggerBucketizerTest, BucketizeUserActivityEventData) {
  UserActivityEvent user_activity_event;
  UserActivityEvent::Event& event = *user_activity_event.mutable_event();
  event.set_log_duration_sec(395);
  event.set_reason(UserActivityEvent::Event::USER_ACTIVITY);
  event.set_type(UserActivityEvent::Event::REACTIVATE);
  event.set_screen_dim_occurred(true);
  event.set_screen_off_occurred(true);
  event.set_screen_lock_occurred(true);

  // In the order of metrics names in ukm.
  UserActivityEvent::Features& features =
      *user_activity_event.mutable_features();
  features.set_battery_percent(96.0);
  features.set_device_management(UserActivityEvent::Features::UNMANAGED);
  features.set_device_mode(UserActivityEvent::Features::CLAMSHELL);
  features.set_device_type(UserActivityEvent::Features::CHROMEBOOK);
  features.set_last_activity_day(UserActivityEvent::Features::MON);
  features.set_last_activity_time_sec(7300);
  features.set_last_user_activity_time_sec(3800);
  features.set_key_events_in_last_hour(20000);
  features.set_recent_time_active_sec(10);
  features.set_previous_negative_actions_count(2);
  features.set_previous_positive_actions_count(1);
  features.set_video_playing_time_sec(800);
  features.set_on_to_dim_sec(100);
  features.set_dim_to_screen_off_sec(200);
  features.set_screen_dimmed_initially(false);
  features.set_screen_locked_initially(false);
  features.set_screen_off_initially(false);
  features.set_time_since_last_mouse_sec(100);
  features.set_time_since_last_touch_sec(311);
  features.set_time_since_video_ended_sec(400);
  features.set_mouse_events_in_last_hour(89);
  features.set_touch_events_in_last_hour(1890);

  std::map<std::string, int> buckets =
      UserActivityUkmLoggerBucketizer::BucketizeUserActivityEventData(
          user_activity_event);
  EXPECT_EQ(9u, buckets.size());
  EXPECT_EQ(95, buckets["BatteryPercent"]);
  EXPECT_EQ(395, buckets["EventLogDuration"]);
  EXPECT_EQ(10000, buckets["KeyEventsInLastHour"]);
  EXPECT_EQ(2, buckets["LastActivityTime"]);
  EXPECT_EQ(1, buckets["LastUserActivityTime"]);
  EXPECT_EQ(89, buckets["MouseEventsInLastHour"]);
  EXPECT_EQ(600, buckets["RecentVideoPlayingTime"]);
  EXPECT_EQ(360, buckets["TimeSinceLastVideoEnded"]);
  EXPECT_EQ(1000, buckets["TouchEventsInLastHour"]);
}

}  // namespace ml
}  // namespace power
}  // namespace ash
