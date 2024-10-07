// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/user_activity_ukm_logger_helpers.h"

#include <array>
#include <cmath>

namespace ash {
namespace power {
namespace ml {

namespace {

constexpr std::array<Bucket, 1> kBatteryPercentBuckets = {{{100, 5}}};

constexpr std::array<Bucket, 3> kEventLogDurationBuckets = {
    {{600, 1}, {1200, 10}, {1800, 20}}};

constexpr std::array<Bucket, 3> kUserInputEventBuckets = {
    {{100, 1}, {1000, 100}, {10000, 1000}}};

constexpr std::array<Bucket, 4> kRecentVideoPlayingTimeBuckets = {
    {{60, 1}, {1200, 300}, {3600, 600}, {18000, 1800}}};

constexpr std::array<Bucket, 5> kTimeSinceLastVideoEndedBuckets = {
    {{60, 1}, {600, 60}, {1200, 300}, {3600, 600}, {18000, 1800}}};

}  // namespace

std::map<std::string, int>
UserActivityUkmLoggerBucketizer::BucketizeUserActivityEventFeatures(
    const UserActivityEvent::Features& features) {
  std::map<std::string, int> buckets;

  if (features.has_battery_percent()) {
    buckets[kBatteryPercent] = Bucketize(std::floor(features.battery_percent()),
                                         kBatteryPercentBuckets);
  }

  if (features.has_key_events_in_last_hour()) {
    buckets[kKeyEventsInLastHour] =
        Bucketize(features.key_events_in_last_hour(), kUserInputEventBuckets);
  }

  buckets[kLastActivityTime] =
      std::floor(features.last_activity_time_sec() / 3600);

  if (features.has_last_user_activity_time_sec()) {
    buckets[kLastUserActivityTime] =
        std::floor(features.last_user_activity_time_sec() / 3600);
  }

  if (features.has_mouse_events_in_last_hour()) {
    buckets[kMouseEventsInLastHour] =
        Bucketize(features.mouse_events_in_last_hour(), kUserInputEventBuckets);
  }

  if (features.has_video_playing_time_sec()) {
    buckets[kRecentVideoPlayingTime] = Bucketize(
        features.video_playing_time_sec(), kRecentVideoPlayingTimeBuckets);
  }

  if (features.has_time_since_video_ended_sec()) {
    buckets[kTimeSinceLastVideoEnded] = Bucketize(
        features.time_since_video_ended_sec(), kTimeSinceLastVideoEndedBuckets);
  }

  if (features.has_touch_events_in_last_hour()) {
    buckets[kTouchEventsInLastHour] =
        Bucketize(features.touch_events_in_last_hour(), kUserInputEventBuckets);
  }

  return buckets;
}

std::map<std::string, int>
UserActivityUkmLoggerBucketizer::BucketizeUserActivityEventData(
    const UserActivityEvent& event) {
  std::map<std::string, int> buckets =
      BucketizeUserActivityEventFeatures(event.features());

  if (event.event().has_log_duration_sec()) {
    buckets[kEventLogDuration] =
        Bucketize(event.event().log_duration_sec(), kEventLogDurationBuckets);
  }
  return buckets;
}

}  // namespace ml
}  // namespace power
}  // namespace ash
