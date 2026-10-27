// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_FEATURES_H_
#define CHROME_BROWSER_CHILD_MODULE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace child_module::features {

// Enables dynamic child module detection and patching.
BASE_DECLARE_FEATURE(kDynamicPatching);

// Maximum number of consecutive fast crashes before a module version is
// blocked. A value of 0 or less disables the circuit breaker.
inline constexpr base::FeatureParam<int> kCircuitBreakerMaxConsecutiveCrashes{
    &kDynamicPatching, "circuit_breaker_max_consecutive_crashes", 3};

// Process uptime threshold to distinguish module incompatibilities from normal
// runtime crashes. 30 seconds is chosen as a heuristic: within this window, a
// page typically exercises the primary Mojo interfaces and module code paths it
// needs. Failures (crashes, CIG failures, or bad messages) occurring within
// this threshold increment the consecutive failure counter; renderers that run
// stably beyond this window demonstrate baseline functionality and reset the
// counter.
inline constexpr base::FeatureParam<base::TimeDelta>
    kCircuitBreakerFastCrashThreshold{&kDynamicPatching,
                                      "circuit_breaker_fast_crash_threshold",
                                      base::Seconds(30)};

}  // namespace child_module::features

#endif  // CHROME_BROWSER_CHILD_MODULE_FEATURES_H_
