// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_features.h"

namespace finds::features {

BASE_FEATURE(kChromeFinds, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/493992939): Remove and merge usages under the main flag.
BASE_FEATURE(kChromeFindsInternals, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kModelExecutionCooldownDurationInDays{
    &kChromeFinds, "model_execution_cooldown_duration_in_days",
    /*default_value=*/7};

constexpr base::FeatureParam<int> kThemeCooldownDurationInDays{
    &kChromeFinds, "theme_cooldown_duration_in_days", /*default_value=*/28};

constexpr base::FeatureParam<int> kNotificationStartTimeMinutes{
    &kChromeFinds, "finds_notification_schedule_start_time_minutes",
    /*default_value=*/120};

constexpr base::FeatureParam<int> kNotificationWindowTimeMinutes{
    &kChromeFinds, "finds_notification_schedule_window_time_minutes",
    /*default_value=*/120};

}  // namespace finds::features
