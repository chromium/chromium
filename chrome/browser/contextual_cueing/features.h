// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_FEATURES_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace contextual_cueing {

BASE_DECLARE_FEATURE(kContextualCueingV2);
extern const base::FeatureParam<double> kEduClassifierThreshold;
extern const base::FeatureParam<double> kShoppingClassifierThreshold;
extern const base::FeatureParam<int> kMaxNumBackgroundTabs;

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_FEATURES_H_
