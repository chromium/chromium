// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_YIELD_TO_LOOPER_CHECKER_H_
#define BASE_ANDROID_YIELD_TO_LOOPER_CHECKER_H_

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}

namespace base::android {

// When enabled, periodically yield to Android Looper in MessagePumpAndroid.
// Yield only on UI thread.
BASE_EXPORT BASE_DECLARE_FEATURE(kYieldToLooperPeriodically);

// Amount to wait between yields when `kYieldToLooperPeriodically` is enabled.
BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kYieldToLooperPeriod);

// Tracks scenarios in which the MessagePumpAndroid should yield to the Android
// Looper. Currently yields:
// * After every task while an embedder's startup is running;
// * Periodically, when `kYieldToLooperPeriodically` is enabled.
//
// Must be constructed on the UI thread. All public methods must be called on
// the UI thread.
class BASE_EXPORT YieldToLooperChecker {
 public:
  static YieldToLooperChecker& GetInstance();

  // Initializes feature state after FeatureList initialization.
  void InitializeFeatures();

  // Updates the checker on the startup status.
  void SetStartupRunning(bool is_startup_running);

  // Records the start of a `DoNonDelayedLooperWork()` batch on the UI thread.
  void BeginNonDelayedLooperWork();

  // Returns true when `MessagePumpAndroid::DoNonDelayedLooperWork()` should
  // yield to the Android Looper before running more immediate work.
  bool ShouldYield();

  // Sets a custom `TickClock` for testing. `tick_clock` is retained by the
  // checker and must outlive it or be reset. Passing nullptr resets to the
  // default tick clock.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 private:
  YieldToLooperChecker();
  ~YieldToLooperChecker() = default;

  bool is_startup_running_ = false;
  base::TimeTicks batch_start_;
  // TimeDelta::Max() when periodic yielding is disabled.
  base::TimeDelta yield_period_ = base::TimeDelta::Max();
  raw_ptr<const base::TickClock> tick_clock_;
  THREAD_CHECKER(thread_checker_);

  friend class base::NoDestructor<YieldToLooperChecker>;
};

}  // namespace base::android

#endif  // BASE_ANDROID_YIELD_TO_LOOPER_CHECKER_H_
