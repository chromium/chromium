// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/yield_to_looper_checker.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"

namespace base::android {

BASE_FEATURE(kYieldToLooperPeriodically, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kYieldToLooperPeriod,
                   &kYieldToLooperPeriodically,
                   base::Milliseconds(100));

YieldToLooperChecker::YieldToLooperChecker()
    : tick_clock_(base::DefaultTickClock::GetInstance()) {}

void YieldToLooperChecker::InitializeFeatures() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  yield_period_ = base::FeatureList::IsEnabled(kYieldToLooperPeriodically)
                      ? kYieldToLooperPeriod.Get()
                      : base::TimeDelta::Max();
}

void YieldToLooperChecker::SetStartupRunning(bool is_startup_running) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_startup_running_ = is_startup_running;
}

void YieldToLooperChecker::BeginNonDelayedLooperWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!yield_period_.is_max()) {
    batch_start_ = tick_clock_->NowTicks();
  }
}

bool YieldToLooperChecker::ShouldYield() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_startup_running_) {
    return true;
  }
  if (yield_period_.is_max() || batch_start_.is_null()) {
    return false;
  }
  // Yield every `yield_period_` after `batch_start_` time.
  return (tick_clock_->NowTicks() - batch_start_) >= yield_period_;
}

void YieldToLooperChecker::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  tick_clock_ = tick_clock ? tick_clock : base::DefaultTickClock::GetInstance();
}

// static
YieldToLooperChecker& YieldToLooperChecker::GetInstance() {
  static NoDestructor<YieldToLooperChecker> checker;
  return *checker.get();
}

}  // namespace base::android
