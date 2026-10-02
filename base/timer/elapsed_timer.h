// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TIMER_ELAPSED_TIMER_H_
#define BASE_TIMER_ELAPSED_TIMER_H_

#include <optional>

#include "base/base_export.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

// A simple wrapper around TimeTicks::Now().
class BASE_EXPORT ElapsedTimer {
 public:
  ElapsedTimer();

  ElapsedTimer(const ElapsedTimer&) = default;
  ElapsedTimer& operator=(const ElapsedTimer&) = default;

  // Returns the time elapsed since object construction. If `now_out` is
  // provided, it is populated with the TimeTicks timestamp sampled by this
  // method.
  TimeDelta Elapsed(TimeTicks* now_out = nullptr) const;

  // Returns the timestamp of the creation of this timer.
  TimeTicks start_time() const { return start_time_; }

 private:
  TimeTicks start_time_;
};

// A simple wrapper around ThreadTicks::Now().
class BASE_EXPORT ElapsedThreadTimer {
 public:
  ElapsedThreadTimer();

  ElapsedThreadTimer(const ElapsedThreadTimer&) = default;
  ElapsedThreadTimer& operator=(const ElapsedThreadTimer&) = default;

  // Returns the ThreadTicks time elapsed since object construction.
  // Only valid if |is_supported()| returns true, otherwise returns TimeDelta().
  // If `now_out` is provided, it is populated with the ThreadTicks timestamp
  // sampled by this method.
  TimeDelta Elapsed(ThreadTicks* now_out = nullptr) const;

  // Returns the timestamp of the creation of this timer.
  ThreadTicks start_time() const { return begin_; }

  bool is_supported() const { return is_supported_; }

 private:
  bool is_supported_;
  ThreadTicks begin_;
};

// A simple wrapper around LiveTicks::Now().
class BASE_EXPORT ElapsedLiveTimer {
 public:
  ElapsedLiveTimer();

  ElapsedLiveTimer(const ElapsedLiveTimer&) = default;
  ElapsedLiveTimer& operator=(const ElapsedLiveTimer&) = default;

  // Returns the time elapsed since object construction, not including time that
  // the system was suspended. If `now_out` is provided, it is populated with
  // the LiveTicks timestamp sampled by this method.
  TimeDelta Elapsed(LiveTicks* now_out = nullptr) const;

  // Returns the timestamp of the creation of this timer.
  LiveTicks start_time() const { return start_time_; }

 private:
  LiveTicks start_time_;
};

// A timer that returns std::nullopt from Elapsed() if the OS was suspended
// (system sleep) during the measured interval. Use this for filtering out
// anomalous data from timer-based metrics. Even though LiveTicks
// measurements are somewhat valid across sleeps because the clock pauses
// in synchrony with what's being measured, system suspension periods remain
// a source of noise due to system state transition overhead and
// intermediate idle states.
//
// Sleep detection is best-effort:
// - Returns std::nullopt if system sleep was detected, or if sleep detection
//   was inconclusive due to high clock sampling error (e.g. repeated thread
//   preemption during clock reads exceeding `kMaxAllowedSamplingError`).
// - On platforms with low-power idle states where LiveTicks may not pause
//   (e.g. Windows "Modern Standby"), such states might not be detected (see
//   TODO below).
//
// TODO(crbug.com/567134137): Fully define which suspension states are
// undetected, if any, for example "Modern Standby" on Windows.
//
// Usage Example:
//   ElapsedNoSleepTimer timer;
//   // Do something that takes some time...
//   if (auto elapsed = timer.Elapsed()) {
//     UMA_HISTOGRAM_TIMES("MyTimer", *elapsed);
//   }
class BASE_EXPORT ElapsedNoSleepTimer {
 public:
  // If LiveAndRealTicks.max_error exceeds this threshold, the timer will
  // return std::nullopt from Elapsed(). This threshold value was chosen to
  // be large enough to tolerate brief sub-quantum pre-emptions (e.g high
  // priority lightweight events, and inter-core load balancing) while still
  // being small enough to reliably detect thread suspensions that might
  // affect sleep detection reliability. The policy is: when in doubt, reject
  // rather than return potentially-erroneous data.
  static constexpr TimeDelta kMaxAllowedSamplingError = Microseconds(200);

  ElapsedNoSleepTimer();

  ElapsedNoSleepTimer(const ElapsedNoSleepTimer&) = default;
  ElapsedNoSleepTimer& operator=(const ElapsedNoSleepTimer&) = default;

  // Returns the time elapsed since object construction, or std::nullopt if the
  // system slept during the measured interval or if sleep detection was
  // inconclusive due to high clock sampling error. If `now_out` is provided,
  // it is populated with the LiveTicks timestamp sampled by this method.
  std::optional<TimeDelta> Elapsed(LiveTicks* now_out = nullptr) const;

  // Returns the timestamp of the creation of this timer.
  LiveTicks start_time() const { return start_sample_.live; }

 private:
  explicit ElapsedNoSleepTimer(time_internal::LiveAndRealTicks sample);

  time_internal::LiveAndRealTicks start_sample_;
};

// Whenever there's a ScopedMockElapsedTimersForTest in scope, every
// ElapsedTimer (and variants like ElapsedThreadTimer, ElapsedLiveTimer, and
// ElapsedNoSleepTimer) will always return kMockElapsedTime from Elapsed(). This
// is useful, for example, in unit tests that verify that their impl records
// timing histograms. It enables such tests to observe reliable timings.
class BASE_EXPORT ScopedMockElapsedTimersForTest {
 public:
  static constexpr TimeDelta kMockElapsedTime = Milliseconds(1337);

  // ScopedMockElapsedTimersForTest is not thread-safe (it must be instantiated
  // in a test before other threads begin using ElapsedTimers; and it must
  // conversely outlive any usage of ElapsedTimer in that test).
  ScopedMockElapsedTimersForTest();

  ScopedMockElapsedTimersForTest(const ScopedMockElapsedTimersForTest&) =
      delete;
  ScopedMockElapsedTimersForTest& operator=(
      const ScopedMockElapsedTimersForTest&) = delete;

  ~ScopedMockElapsedTimersForTest();
};

}  // namespace base

#endif  // BASE_TIMER_ELAPSED_TIMER_H_
