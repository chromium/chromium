// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_TIMER_LAP_TIMER_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_TIMER_LAP_TIMER_H_

#include "partition_alloc/partition_alloc_base/time/time.h"

namespace partition_alloc::internal::base {

// LapTimer is used to calculate average times per "Lap" in perf tests.
// NextLap increments the lap counter, used in counting the per lap averages.
// If you initialize the LapTimer with a non zero |warmup_laps|, it will ignore
// the times for that many laps at the start.
// If you set the |time_limit| then you can use HasTimeLimitExpired() to see if
// the current accumulated time has crossed that threshold, with an optimization
// that it only tests this every |check_interval| laps.
class LapTimer {
 public:
  enum class TimerMethod {
    // Measures CPU time consumed by the thread running the LapTimer.
    kUseThreadTicks,
    // Measures elapsed wall time (default).
    kUseTimeTicks
  };

  LapTimer(int warmup_laps,
           TimeDelta time_limit,
           int check_interval,
           TimerMethod timing_method = TimerMethod::kUseTimeTicks);
  // Create LapTimer with sensible default values.
  explicit LapTimer(TimerMethod timing_method = TimerMethod::kUseTimeTicks);

  LapTimer(const LapTimer&) = delete;
  LapTimer& operator=(const LapTimer&) = delete;

  // Sets the timer back to its starting state.
  void Reset();
  // Sets the start point to now.
  void Start();
  // Returns true if there are no more warmup laps to do.
  bool IsWarmedUp() const;
  // Advance the lap counter and update the accumulated time.
  // The accumulated time is only updated every check_interval laps.
  // If accumulating then the start point will also be updated.
  void NextLap();
  // Returns true if the stored time has exceeded the time limit specified.
  // May cause a call to Store().
  bool HasTimeLimitExpired() const;
  // The average time taken per lap.
  TimeDelta TimePerLap() const;
  // The number of laps per second.
  float LapsPerSecond() const;
  // The number of laps recorded.
  int NumLaps() const;

 private:
  // Returns true if all lap times have been timed. Only true every n'th
  // lap, where n = check_interval.
  bool HasTimedAllLaps() const;
  // Returns the current accumulated time.
  TimeDelta GetAccumulatedTime() const;

  const int warmup_laps_;
  const TimeDelta time_limit_;
  const int check_interval_;
  const TimerMethod method_;

  ThreadTicks start_thread_ticks_;
  TimeTicks start_time_ticks_;

  ThreadTicks last_timed_lap_end_thread_ticks_;
  TimeTicks last_timed_lap_end_ticks_;

  int num_laps_;
  int remaining_warmups_ = 0;
  int remaining_no_check_laps_ = 0;
};

}  // namespace partition_alloc::internal::base

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_TIMER_LAP_TIMER_H_
