// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_FEATURES_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace contextual_cueing {

BASE_DECLARE_FEATURE(kContextualCueingV2);
BASE_DECLARE_FEATURE(kContextualCueingV2EnforceAgeRestriction);

extern const base::FeatureParam<double> kEduClassifierThreshold;
extern const base::FeatureParam<double> kShoppingClassifierThreshold;
extern const base::FeatureParam<int> kMaxNumBackgroundTabs;

// If true, disable the cue backoff logic.
extern const base::FeatureParam<bool> kDisableCueBackoff;

// The minimum number of page loads that must occur between nudges.
extern const base::FeatureParam<int> kMinPageCountBetweenNudges;
// The minimum amount of time that must pass between nudges.
extern const base::FeatureParam<base::TimeDelta> kMinTimeBetweenNudges;

// A hard cap limiting the number of cues shown to a user over a certain
// duration. The cue can only be shown `kCueCapCount` times for every
// duration of `kCueCapTime`, regardless of whether the cue was dismissed,
// ignored, or clicked on.
extern const base::FeatureParam<base::TimeDelta> kCueCapTime;
extern const base::FeatureParam<int> kCueCapCount;

// The same hard cap limits per origin. ie., x cues per y hours per origin.
extern const base::FeatureParam<base::TimeDelta> kCueCapTimePerOrigin;
extern const base::FeatureParam<int> kCueCapCountPerOrigin;

// Limit on how many recently visited origins should be kept track of. This is
// used to implement nudge constraints per-origin per 24 hour period.
extern const base::FeatureParam<int> kVisitedOriginsLimit;

// The amount of time to wait when a nudge is dismissed following the
// exponential back off rule. The amount of the time to back off each time can
// be computed as: kBackoffTime * (kBackoffMultiplierBase ^ dismissCount).
extern const base::FeatureParam<base::TimeDelta> kBackoffTime;
extern const base::FeatureParam<double> kBackoffMultiplierBase;

// If true, uses private AI to generate cues.
extern const base::FeatureParam<bool> kUsePrivateAi;

// The help center article link.
extern const base::FeatureParam<std::string> kHelpCenterArticleLink;

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_FEATURES_H_
