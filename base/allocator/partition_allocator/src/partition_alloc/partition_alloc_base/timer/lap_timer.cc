// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/timer/lap_timer.h"

#include "partition_alloc/partition_alloc_check.h"

namespace partition_alloc::internal::base {

namespace {

// Default values.
constexpr TimeDelta kDefaultTimeLimit = Seconds(3);
constexpr int kDefaultWarmupRuns = 5;
constexpr int kDefaultTimeCheckInterval = 10;

}  // namespace

LapTimer::LapTimer(int warmup_laps,
                   TimeDelta time_limit,
                   int check_interval,
                   LapTimer::TimerMethod method)
    : warmup_laps_(warmup_laps),
      time_limit_(time_limit),
      check_interval_(check_interval),
      method_(method) {
  PA_DCHECK(check_interval > 0);
  Reset();
}

LapTimer::LapTimer(LapTimer::TimerMethod method)
    : LapTimer(kDefaultWarmupRuns,
               kDefaultTimeLimit,
               kDefaultTimeCheckInterval,
               method) {}

void LapTimer::Reset() {
  if (ThreadTicks::IsSupported() && method_ == TimerMethod::kUseThreadTicks) {
    ThreadTicks::WaitUntilInitialized();
  }
  num_laps_ = 0;
  remaining_warmups_ = warmup_laps_;
  remaining_no_check_laps_ = check_interval_;
  Start();
}

void LapTimer::Start() {
  PA_DCHECK(num_laps_ == 0);
  // last_timed_ variables are initialized here (instead of in the constructor)
  // because not all platforms support ThreadTicks.
  if (method_ == TimerMethod::kUseThreadTicks) {
    start_thread_ticks_ = ThreadTicks::Now();
    last_timed_lap_end_thread_ticks_ = ThreadTicks::Now();
  } else {
    start_time_ticks_ = TimeTicks::Now();
    last_timed_lap_end_ticks_ = TimeTicks::Now();
  }
}

bool LapTimer::IsWarmedUp() const {
  return remaining_warmups_ <= 0;
}

void LapTimer::NextLap() {
  PA_DCHECK(!start_thread_ticks_.is_null() || !start_time_ticks_.is_null());
  if (!IsWarmedUp()) {
    --remaining_warmups_;
    if (IsWarmedUp()) {
      Start();
    }
    return;
  }
  ++num_laps_;
  --remaining_no_check_laps_;
  if (!remaining_no_check_laps_) {
    if (method_ == TimerMethod::kUseTimeTicks) {
      last_timed_lap_end_ticks_ = TimeTicks::Now();
    } else {
      last_timed_lap_end_thread_ticks_ = ThreadTicks::Now();
    }
    remaining_no_check_laps_ = check_interval_;
  }
}

TimeDelta LapTimer::GetAccumulatedTime() const {
  if (method_ == TimerMethod::kUseTimeTicks) {
    return last_timed_lap_end_ticks_ - start_time_ticks_;
  }
  return last_timed_lap_end_thread_ticks_ - start_thread_ticks_;
}

bool LapTimer::HasTimeLimitExpired() const {
  return GetAccumulatedTime() >= time_limit_;
}

bool LapTimer::HasTimedAllLaps() const {
  return num_laps_ && !(num_laps_ % check_interval_);
}

TimeDelta LapTimer::TimePerLap() const {
  PA_DCHECK(HasTimedAllLaps());
  PA_DCHECK(num_laps_ > 0);
  return GetAccumulatedTime() / num_laps_;
}

float LapTimer::LapsPerSecond() const {
  PA_DCHECK(HasTimedAllLaps());
  PA_DCHECK(num_laps_ > 0);
  return num_laps_ / GetAccumulatedTime().InSecondsF();
}

int LapTimer::NumLaps() const {
  return num_laps_;
}

}  // namespace partition_alloc::internal::base
