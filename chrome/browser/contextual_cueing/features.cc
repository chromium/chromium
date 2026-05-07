// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/features.h"

namespace contextual_cueing {

BASE_FEATURE(kContextualCueingV2, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kContextualCueingV2EnforceAgeRestriction,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

const base::FeatureParam<bool> kDisableCueBackoff(
    &kContextualCueingV2,
    "ContextualCueingV2DisableCueBackoff",
    false);

const base::FeatureParam<int> kMinPageCountBetweenNudges(
    &kContextualCueingV2,
    "ContextualCueingV2MinPageCountBetweenNudges",
    10);

const base::FeatureParam<base::TimeDelta> kMinTimeBetweenNudges(
    &kContextualCueingV2,
    "ContextualCueingV2MinTimeBetweenNudges",
    base::Minutes(10));

// Cap number of cues shown to a user to be at most `kCueCapCount` in
// `kCueCapTime` duration.
const base::FeatureParam<base::TimeDelta> kCueCapTime(
    &kContextualCueingV2,
    "ContextualCueingV2CueCapTime",
    base::Hours(8));
const base::FeatureParam<int> kCueCapCount(&kContextualCueingV2,
                                           "ContextualCueingV2CueCapCount",
                                           10);

// Cap number of cues for an origin to be at most `kCueCapCountPerOrigin` in
// `kCueCapTimePerOrigin` duration.
const base::FeatureParam<base::TimeDelta> kCueCapTimePerOrigin(
    &kContextualCueingV2,
    "ContextualCueingV2CueCapTimePerOrigin",
    base::Hours(4));
const base::FeatureParam<int> kCueCapCountPerOrigin(
    &kContextualCueingV2,
    "ContextualCueingV2CueCapCountPerOrigin",
    3);

const base::FeatureParam<int> kVisitedOriginsLimit(
    &kContextualCueingV2,
    "ContextualCueingV2VisitedOriginsLimit",
    20);

const base::FeatureParam<base::TimeDelta> kBackoffTime(
    &kContextualCueingV2,
    "ContextualCueingV2BackoffTime",
    base::Hours(24));

const base::FeatureParam<double> kBackoffMultiplierBase(
    &kContextualCueingV2,
    "ContextualCueingV2BackoffMultiplierBase",
    2.0);

const base::FeatureParam<bool> kUsePrivateAi(
    &kContextualCueingV2,
    "ContextualCueingV2UsePrivateAi",
    false);

const base::FeatureParam<std::string> kHelpCenterArticleLink(
    &kContextualCueingV2,
    "ContextualCueingV2HelpCenterArticleLink",
    "https://support.google.com/chrome?p=");

}  // namespace contextual_cueing
