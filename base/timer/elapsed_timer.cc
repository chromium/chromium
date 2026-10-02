// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/elapsed_timer.h"

#include "base/check.h"

namespace base {

namespace {
bool g_mock_elapsed_timers_for_test = false;
}  // namespace

ElapsedTimer::ElapsedTimer() : start_time_(TimeTicks::Now()) {}

TimeDelta ElapsedTimer::Elapsed(TimeTicks* now_out) const {
  if (g_mock_elapsed_timers_for_test) {
    if (now_out) {
      *now_out = start_time_ + ScopedMockElapsedTimersForTest::kMockElapsedTime;
    }
    return ScopedMockElapsedTimersForTest::kMockElapsedTime;
  }
  const TimeTicks now = TimeTicks::Now();
  if (now_out) {
    *now_out = now;
  }
  return now - start_time_;
}

ElapsedThreadTimer::ElapsedThreadTimer()
    : is_supported_(ThreadTicks::IsSupported()),
      begin_(is_supported_ ? ThreadTicks::Now() : ThreadTicks()) {}

TimeDelta ElapsedThreadTimer::Elapsed(ThreadTicks* now_out) const {
  if (!is_supported_) {
    if (now_out) {
      *now_out = ThreadTicks();
    }
    return TimeDelta();
  }
  if (g_mock_elapsed_timers_for_test) {
    if (now_out) {
      *now_out = begin_ + ScopedMockElapsedTimersForTest::kMockElapsedTime;
    }
    return ScopedMockElapsedTimersForTest::kMockElapsedTime;
  }
  const ThreadTicks now = ThreadTicks::Now();
  if (now_out) {
    *now_out = now;
  }
  return now - begin_;
}

ElapsedLiveTimer::ElapsedLiveTimer() : start_time_(LiveTicks::Now()) {}

TimeDelta ElapsedLiveTimer::Elapsed(LiveTicks* now_out) const {
  if (g_mock_elapsed_timers_for_test) {
    if (now_out) {
      *now_out = start_time_ + ScopedMockElapsedTimersForTest::kMockElapsedTime;
    }
    return ScopedMockElapsedTimersForTest::kMockElapsedTime;
  }
  const LiveTicks now = LiveTicks::Now();
  if (now_out) {
    *now_out = now;
  }
  return now - start_time_;
}

// static
constexpr TimeDelta ElapsedNoSleepTimer::kMaxAllowedSamplingError;

ElapsedNoSleepTimer::ElapsedNoSleepTimer()
    : start_sample_(time_internal::SampleLiveAndRealTicks()) {}

ElapsedNoSleepTimer::ElapsedNoSleepTimer(time_internal::LiveAndRealTicks sample)
    : start_sample_(sample) {}

std::optional<TimeDelta> ElapsedNoSleepTimer::Elapsed(
    LiveTicks* now_out) const {
  if (g_mock_elapsed_timers_for_test) {
    if (now_out) {
      *now_out =
          start_sample_.live + ScopedMockElapsedTimersForTest::kMockElapsedTime;
    }
    return ScopedMockElapsedTimersForTest::kMockElapsedTime;
  }
  const time_internal::LiveAndRealTicks current_sample =
      time_internal::SampleLiveAndRealTicks();
  if (now_out) {
    *now_out = current_sample.live;
  }
  const TimeDelta live_elapsed = current_sample.live - start_sample_.live;
  const TimeDelta real_elapsed = current_sample.real - start_sample_.real;

  // The sampling uncertainty between the two clocks in each sample is bounded
  // by `max_error`. Across both start and end samples, the maximum difference
  // between `live_elapsed` and `real_elapsed` attributable to non-atomic
  // reads cannot exceed the sum of their individual read error bounds.
  const TimeDelta max_error =
      start_sample_.max_error + current_sample.max_error;

  // If clock sampling uncertainty is too large (e.g. thread preemption
  // occurred between clock reads in all retry attempts during start or end
  // sampling), sleep detection is inconclusive and interval timing is noisy.
  // Return nullopt to discard this sample.
  if (max_error > kMaxAllowedSamplingError) {
    return std::nullopt;
  }

  if (real_elapsed - live_elapsed > max_error) {
    return std::nullopt;
  }
  return live_elapsed;
}

// static
constexpr TimeDelta ScopedMockElapsedTimersForTest::kMockElapsedTime;

ScopedMockElapsedTimersForTest::ScopedMockElapsedTimersForTest() {
  DCHECK(!g_mock_elapsed_timers_for_test);
  g_mock_elapsed_timers_for_test = true;
}

ScopedMockElapsedTimersForTest::~ScopedMockElapsedTimersForTest() {
  DCHECK(g_mock_elapsed_timers_for_test);
  g_mock_elapsed_timers_for_test = false;
}

}  // namespace base
