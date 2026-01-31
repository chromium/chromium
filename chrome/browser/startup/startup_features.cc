// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/startup/startup_features.h"

namespace features {

BASE_FEATURE(kLaunchOnStartup, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_ENUM_PARAM(LaunchOnStartupMode,
                        kLaunchOnStartupModeParam,
                        &kLaunchOnStartup,
                        "mode",
                        LaunchOnStartupMode::kForeground,
                        kLaunchOnStartupModeOptions);

BASE_FEATURE_ENUM_PARAM(LaunchOnStartupDefaultPreference,
                        kLaunchOnStartupDefaultPreferenceParam,
                        &kLaunchOnStartup,
                        "default_preference",
                        LaunchOnStartupDefaultPreference::kDisabled,
                        kLaunchOnStartupTrialGroupOptions);

bool IsForegroundLaunchEnabled() {
  return base::FeatureList::IsEnabled(kLaunchOnStartup) &&
         kLaunchOnStartupModeParam.Get() == LaunchOnStartupMode::kForeground;
}

LaunchOnStartupDefaultPreference GetLaunchOnStartupDefaultPreference() {
  CHECK(base::FeatureList::IsEnabled(kLaunchOnStartup));

  return kLaunchOnStartupDefaultPreferenceParam.Get();
}

}  // namespace features
