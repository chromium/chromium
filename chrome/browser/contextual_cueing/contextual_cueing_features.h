// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_FEATURES_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace contextual_cueing {

BASE_DECLARE_FEATURE(kContextualCueing);

// The amount of time to wait when a nudge is dismissed following the
// exponential back off rule. The amount of the time to back off each time can
// be computed as: kBackoffTime * (kBackoffMultiplierBase ^ dismissCount).
extern const base::FeatureParam<base::TimeDelta> kBackoffTime;
extern const base::FeatureParam<double> kBackoffMultiplierBase;

// A hard cap limiting the number of nudges shown to a user over a certain
// duration. The nudge can only be shown `kNudgeCapCount` times for every
// duration of `kNudgeCapTime`, regardless of whether the nudge was dismissed,
// ignored, or clicked on.
extern const base::FeatureParam<base::TimeDelta> kNudgeCapTime;
extern const base::FeatureParam<int> kNudgeCapCount;

// A hard limit of page navigation counts between nudges. After one nudge is
// shown, there must be at least `kMinPageCountBetweenNudges` page navigations
// before the next nudge can be shown.
extern const base::FeatureParam<int> kMinPageCountBetweenNudges;

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_FEATURES_H_
