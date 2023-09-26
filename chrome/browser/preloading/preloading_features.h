// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_PRELOADING_FEATURES_H_
#define CHROME_BROWSER_PRELOADING_PRELOADING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

BASE_DECLARE_FEATURE(kPerformanceSettingsPreloadingSubpage);

// Whether the v2 UI for the preloading page is shown in performance settings.
extern const base::FeatureParam<bool> kPerformanceSettingsPreloadingSubpageV2;

}  // namespace features

#endif  // CHROME_BROWSER_PRELOADING_PRELOADING_FEATURES_H_
