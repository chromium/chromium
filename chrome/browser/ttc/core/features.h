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

// Finch-configurable interval between periodic playback status reports during
// a model turn.
inline constexpr base::FeatureParam<base::TimeDelta>
    kPlaybackStatusReportInterval{&kTtc, "playback_status_report_interval",
                                  base::Milliseconds(200)};

enum class TtcPageContentMode {
  kDefault,
  kActionable,
};

// "default" here refers to APC default. Actionable is the Ttc's default fetch
// mode.
inline constexpr base::FeatureParam<TtcPageContentMode>::Option
    kTtcPageContentModeOptions[] = {
        {TtcPageContentMode::kDefault, "default"},
        {TtcPageContentMode::kActionable, "actionable"},
};

inline constexpr base::FeatureParam<TtcPageContentMode> kTtcPageContentMode{
    &kTtc, "apc_mode", TtcPageContentMode::kActionable,
    &kTtcPageContentModeOptions};

// Enables the first run experience feature for TTC. While FRE is under
// development, this is used to hide the unfinished FRE. Once FRE is complete,
// this can be used to suppress the feature in tests.
BASE_DECLARE_FEATURE(kTtcFre);

// Enables chrome://ttc-internals, which records TTC sessions in memory
// (pipeline health, timeline, tool calls, problems and audio) for debugging.
BASE_DECLARE_FEATURE(kTtcDebugTools);

// Enables Human-on-the-Loop (HOTL) local user confirmation before executing
// click_element tool actions.
BASE_DECLARE_FEATURE(kTtcHotlClickConfirmation);

// Finch-configurable timeout waiting for the user to respond to a HOTL
// confirmation prompt via voice or UI button click.
inline constexpr base::FeatureParam<base::TimeDelta> kHotlConfirmationTimeout{
    &kTtcHotlClickConfirmation, "confirmation_timeout", base::Seconds(10)};

// Offers the model a get_pointer tool that resolves where the user's pointer
// has been during their turn, so "what is this?" can be answered. Design:
// go/ttc-get-pointer.
BASE_DECLARE_FEATURE(kTtcGetPointer);

// Thresholds for resolving where the user pointed; see PointerTracker::Config.
inline constexpr base::FeatureParam<double> kTtcGetPointerDwellRadiusDip{
    &kTtcGetPointer, "dwell_radius_dip", 12.0};
inline constexpr base::FeatureParam<base::TimeDelta>
    kTtcGetPointerMinDwellDuration{&kTtcGetPointer, "min_dwell_duration",
                                   base::Milliseconds(300)};
inline constexpr base::FeatureParam<base::TimeDelta>
    kTtcGetPointerPreTurnLookback{&kTtcGetPointer, "pre_turn_lookback",
                                  base::Seconds(1)};
inline constexpr base::FeatureParam<base::TimeDelta> kTtcGetPointerIdleLimit{
    &kTtcGetPointer, "idle_limit", base::Seconds(15)};
// The most locations get_pointer reports; more than one serves "compare this
// with that".
inline constexpr base::FeatureParam<int> kTtcGetPointerMaxResults{
    &kTtcGetPointer, "max_results", 1};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_FEATURES_H_
