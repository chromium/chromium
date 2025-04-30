// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"

#include "base/metrics/field_trial_params.h"

namespace contextual_cueing {

BASE_FEATURE(kContextualCueing,
             "ContextualCueing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicZeroStateSuggestions,
             "GlicZeroStateSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kBackoffTime(&kContextualCueing,
                                                       "BackoffTime",
                                                       base::Hours(24));

const base::FeatureParam<double> kBackoffMultiplierBase(&kContextualCueing,
                                                        "BackoffMultiplierBase",
                                                        2.0);

const base::FeatureParam<base::TimeDelta> kNudgeCapTime(&kContextualCueing,
                                                        "NudgeCapTime",
                                                        base::Hours(8));

const base::FeatureParam<int> kNudgeCapCount(&kContextualCueing,
                                             "NudgeCapCount",
                                             10);

const base::FeatureParam<base::TimeDelta> kNudgeCapTimePerDomain(
    &kContextualCueing,
    "NudgeCapTimePerDomain",
    base::Hours(8));

const base::FeatureParam<int> kNudgeCapCountPerDomain(&kContextualCueing,
                                                      "NudgeCapCountPerDomain",
                                                      3);

const base::FeatureParam<int> kMinPageCountBetweenNudges(
    &kContextualCueing,
    "MinPageCountBetweenNudges",
    3);

const base::FeatureParam<base::TimeDelta> kMinTimeBetweenNudges(
    &kContextualCueing,
    "MinTimeBetweenNudges",
    base::Minutes(10));

const base::FeatureParam<int> kVisitedDomainsLimit(&kContextualCueing,
                                                   "VisitedDomainsLimit",
                                                   20);

const base::FeatureParam<base::TimeDelta> kPdfPageCountCaptureDelay(
    &kContextualCueing,
    "PdfPageCountCaptureDelay",
    base::Seconds(4));

const base::FeatureParam<bool> kEnablePageContentExtraction(
    &kContextualCueing,
    "EnablePageContentExtraction",
    true);

const base::FeatureParam<bool> kExtractInnerTextForZeroStateSuggestions(
    &kGlicZeroStateSuggestions,
    "ZSSExtractInnerText",
    true);

const base::FeatureParam<bool>
    kExtractAnnotatedPageContentForZeroStateSuggestions(
        &kGlicZeroStateSuggestions,
        "ZSSExtractAnnotatedPageContent",
        false);

const base::FeatureParam<base::TimeDelta>
    kPageContentExtractionDelayForSameDocumentNavigation(
        &kGlicZeroStateSuggestions,
        "ZSSPageContentExtractionDelayForSameDocumentNavigation",
        base::Seconds(3));

const base::FeatureParam<bool> kReturnEmptyForSameDocumentNavigation(
    &kGlicZeroStateSuggestions,
    "ZSSReturnEmptyForSameDocumentNavigation",
    false);

}  // namespace contextual_cueing
