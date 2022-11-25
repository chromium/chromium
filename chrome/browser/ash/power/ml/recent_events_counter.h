// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_RECENT_EVENTS_COUNTER_H_
#define CHROME_BROWSER_ASH_POWER_ML_RECENT_EVENTS_COUNTER_H_

#include <vector>

#include "base/time/time.h"

namespace ash {
namespace power {
namespace ml {

// RecentEventsCounter keeps a running count of events that occurred in the last
// |duration| period of time. For example, a count of the number of events in
// the last hour.
//
// Rather than remembering the time stamp for each event, the event times are
// bucketed. The number of requested buckets must exactly divide a time period
// of |duration_| (within the precision of TimeDelta), and initially start at
// base::TimeDelta(). For logging at a time later than |duration_|, the buckets
// are reused, using the logging time modulo the |duration_| in the calculation
// of the bucket to be used. The total is calculated by keeping track of the
// |first_bucket_index_| and |first_bucket_time_| and zeroing buckets with stale
// data.
//
// The bucketing determines the time precision of the count. This
// means that the actual time period counted may be up to one bucket length
// shorter than the requested time period. It will never be longer than the
// requested time period.
class RecentEventsCounter {
 public:
  // Count events for a time period of length |duration| using
  // |num_buckets| buckets.
  RecentEventsCounter(base::TimeDelta duration, int num_buckets);

  RecentEventsCounter(const RecentEventsCounter&) = delete;
  RecentEventsCounter& operator=(const RecentEventsCounter&) = delete;

  ~RecentEventsCounter();

  // Log an event at timedelta |timestamp|. |timestamp| cannot be negative.
  void Log(base::TimeDelta timestamp);

  // Return the count of events reported in the |duration_| preceding |now|.
  // |now| must be >= any |timestamp| previously passed to Log().
  int GetTotal(base::TimeDelta now) const;

 private:
  // Return the index of the bucket containing |timestamp|.
  int GetBucketIndex(base::TimeDelta timestamp) const;

  // The length of time that events should be recorded.
  const base::TimeDelta duration_;
  // The number of buckets to use to record the events.
  const int num_buckets_;
  // The number of events in each bucket.
  std::vector<int> event_count_;
  // The index of the first bucket. |event_count_| is a circular array.
  int first_bucket_index_ = 0;
  // The starting time of the first bucket.
  base::TimeDelta first_bucket_time_;
  // The duration of each bucket.
  base::TimeDelta bucket_duration_;
  // The latest timedelta that has been logged.
  base::TimeDelta latest_;
};

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_RECENT_EVENTS_COUNTER_H_
