// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_features.h"

namespace finds::features {

BASE_FEATURE(kChromeFinds, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/493992939): Remove and merge usages under the main flag.
BASE_FEATURE(kChromeFindsInternals, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kModelExecutionCooldownDurationInDays{
    &kChromeFinds, "model_execution_cooldown_duration_in_days", 7};

const base::FeatureParam<int> kThemeCooldownDurationInDays{
    &kChromeFinds, "theme_cooldown_duration_in_days", 28};

}  // namespace finds::features
