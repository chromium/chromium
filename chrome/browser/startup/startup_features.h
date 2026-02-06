// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_STARTUP_FEATURES_H_
#define CHROME_BROWSER_STARTUP_STARTUP_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Launch modes for the feature.
//
// 1. Foreground mode param gates launch on foreground feature and the settings
// toggle UI to enable/disable it.
//
// LINT.IfChange(LaunchOnStartupMode)
enum class LaunchOnStartupMode { kForeground };
// LINT.ThenChange(//tools/metrics/histograms/metadata/startup/histograms.xml:StartupLaunchMode)

// Default preference value for running experiment with the feature.
//
// 1. Disabled group gets the preference set to false by default, with a UI
// surface offering user to opt-in.
//
// 2. Enabled group gets the preference set to true by default, with a UI
// surface offering user to opt-out.
//
// 3. Control group will be denoted by turning the root feature flag off.
enum class LaunchOnStartupDefaultPreference { kDisabled, kEnabled };

// Association of feature param names to enum members.
// Array order must match order of enum members.
constexpr inline auto kLaunchOnStartupModeOptions =
    std::to_array<base::FeatureParam<LaunchOnStartupMode>::Option>({
        {LaunchOnStartupMode::kForeground, "foreground"},
    });

constexpr inline auto kLaunchOnStartupTrialGroupOptions =
    std::to_array<base::FeatureParam<LaunchOnStartupDefaultPreference>::Option>(
        {
            {LaunchOnStartupDefaultPreference::kDisabled, "disabled"},
            {LaunchOnStartupDefaultPreference::kEnabled, "enabled"},
        });

// This flag (and the params) gates the Launch on Startup feature and
// corresponding modes.
BASE_DECLARE_FEATURE(kLaunchOnStartup);
BASE_DECLARE_FEATURE_PARAM(LaunchOnStartupMode, kLaunchOnStartupModeParam);
BASE_DECLARE_FEATURE_PARAM(LaunchOnStartupDefaultPreference,
                           kLaunchOnStartupDefaultPreferenceParam);

// Returns whether the foreground launch feature is enabled.
bool IsForegroundLaunchEnabled();

// Returns the default preference value of the user based on finch config.
// This method will crash if the `kLaunchOnStartup` feature flag is disabled.
LaunchOnStartupDefaultPreference GetLaunchOnStartupDefaultPreference();

}  // namespace features

#endif  // CHROME_BROWSER_STARTUP_STARTUP_FEATURES_H_
