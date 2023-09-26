// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preloading_features.h"

namespace features {

BASE_FEATURE(kPerformanceSettingsPreloadingSubpage,
             "PerformanceSettingsPreloadingSubpage",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kPerformanceSettingsPreloadingSubpageV2{
    &kPerformanceSettingsPreloadingSubpage, "use_v2_preloading_subpage", true};

}  // namespace features
