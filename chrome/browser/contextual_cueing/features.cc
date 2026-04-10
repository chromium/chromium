// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/features.h"

namespace contextual_cueing {

BASE_FEATURE(kContextualCueingV2, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<double> kEduClassifierThreshold(
    &kContextualCueingV2,
    "ContextualCueingV2EduClassifierThreshold",
    0.5);

const base::FeatureParam<double> kShoppingClassifierThreshold(
    &kContextualCueingV2,
    "ContextualCueingV2ShoppingClassifierThreshold",
    0.5);

const base::FeatureParam<int> kMaxNumBackgroundTabs(
    &kContextualCueingV2,
    "ContextualCueingV2MaxNumBackgroundTabs",
    10);

}  // namespace contextual_cueing
