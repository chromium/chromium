// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_HELPERS_H_
#define CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_HELPERS_H_

#include <map>
#include <string>

#include "base/check_op.h"
#include "base/logging.h"
#include "chrome/browser/ash/power/ml/user_activity_event.pb.h"

namespace ash {
namespace power {
namespace ml {

// Metrics below are bucketized.
constexpr char kBatteryPercent[] = "BatteryPercent";
constexpr char kEventLogDuration[] = "EventLogDuration";
constexpr char kKeyEventsInLastHour[] = "KeyEventsInLastHour";
constexpr char kLastActivityTime[] = "LastActivityTime";
constexpr char kLastUserActivityTime[] = "LastUserActivityTime";
constexpr char kMouseEventsInLastHour[] = "MouseEventsInLastHour";
constexpr char kRecentVideoPlayingTime[] = "RecentVideoPlayingTime";
constexpr char kTimeSinceLastVideoEnded[] = "TimeSinceLastVideoEnded";
constexpr char kTouchEventsInLastHour[] = "TouchEventsInLastHour";

// TODO(jiameng): both Bucket and Bucketize are meant for user activity logging,
// but it's currently used by adaptive brightness. Need to refactor by either
// moving these two items to a more common lib or having a helper file for
// adaptive brightness as the two projects are likely to diverge.

// Both |boundary_end| and |rounding| must be positive.
struct Bucket {
  int boundary_end;
  int rounding;
};

// Bucketize |original_value| using given |buckets|, which is an array of
// Bucket and must be sorted in ascending order of |boundary_end|.
// |original_value| must be non-negative. An example of |buckets| is
// {{60, 1}, {300, 10}, {600, 20}}. This function rounds |original_value| down
// to the nearest |bucket.rounding|, where |bucket| is the first entry in
// |buckets| with |bucket.boundary_end| > |original_value|.
// If |original_value| is greater than all |boundary_end|, the function
// returns the largest |boundary_end|. Using the above |buckets| example, the
// function will return 30 if |original_value| = 30, and 290 if
// |original_value| = 299.
template <size_t N>
int Bucketize(int original_value, const std::array<Bucket, N>& buckets) {
  DCHECK_GE(original_value, 0);
  DCHECK(!buckets.empty());
  for (const auto& bucket : buckets) {
    if (original_value < bucket.boundary_end) {
      return bucket.rounding * (original_value / bucket.rounding);
    }
  }
  return buckets.back().boundary_end;
}

class UserActivityUkmLoggerBucketizer {
 public:
  UserActivityUkmLoggerBucketizer() = delete;
  UserActivityUkmLoggerBucketizer(const UserActivityUkmLoggerBucketizer&) =
      delete;
  UserActivityUkmLoggerBucketizer& operator=(
      const UserActivityUkmLoggerBucketizer&) = delete;

  // Bucketizes features if they are present. Returns a
  // feature->bucketized_value map.
  static std::map<std::string, int> BucketizeUserActivityEventFeatures(
      const UserActivityEvent::Features& features);

  // Bucketizes features and also EventLogDuration.
  static std::map<std::string, int> BucketizeUserActivityEventData(
      const UserActivityEvent& event);
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_USER_ACTIVITY_UKM_LOGGER_HELPERS_H_
