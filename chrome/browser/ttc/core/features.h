// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_FEATURES_H_
#define CHROME_BROWSER_TTC_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/ttc/features.h"  // IWYU pragma: export

namespace ttc {

// Finch-configurable inactivity timeout before closing the active listening
// session.
inline constexpr base::FeatureParam<base::TimeDelta> kSessionInactivityTimeout{
    &kTtc, "session_inactivity_timeout", base::Minutes(5)};

// Finch-configurable audio energy threshold [0.0, 1.0] to distinguish user
// speech from ambient silence.
inline constexpr base::FeatureParam<double>
    kSessionInactivityAudioEnergyThreshold{
        &kTtc, "session_inactivity_audio_energy_threshold", 0.05};

// Enables the first run experience feature for TTC. While FRE is under
// development, this is used to hide the unfinished FRE. Once FRE is complete,
// this can be used to suppress the feature in tests.
BASE_DECLARE_FEATURE(kTtcFre);

// Enables chrome://ttc-internals, which records TTC sessions in memory
// (pipeline health, timeline, tool calls, problems and audio) for debugging.
BASE_DECLARE_FEATURE(kTtcDebugTools);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_FEATURES_H_
