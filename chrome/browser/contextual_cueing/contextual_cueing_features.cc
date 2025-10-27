// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "components/variations/service/variations_service.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace contextual_cueing {

BASE_FEATURE(kContextualCueing, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlicZeroStateSuggestions, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsContextualCueingEnabled() {
#if BUILDFLAG(ENABLE_GLIC)
  // If the feature is overridden (e.g. via server-side config or command-line),
  // use that state.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverridden(kContextualCueing.name)) {
    // Important: If a server-side config applies to this client (i.e. after
    // accounting for its filters), but the client gets assigned to the default
    // group, they will still take this code path and receive the state
    // specified via BASE_FEATURE() above.
    return base::FeatureList::IsEnabled(kContextualCueing);
  }

  return glic::GlicEnabling::IsInRolloutLocation();
#else
  return base::FeatureList::IsEnabled(kContextualCueing);
#endif
}

bool IsZeroStateSuggestionsEnabled() {
#if BUILDFLAG(ENABLE_GLIC)
  // If the feature is overridden (e.g. via server-side config or command-line),
  // use that state.
  auto* feature_list = base::FeatureList::GetInstance();
  if (feature_list &&
      feature_list->IsFeatureOverridden(kGlicZeroStateSuggestions.name)) {
    // Important: If a server-side config applies to this client (i.e. after
    // accounting for its filters), but the client gets assigned to the default
    // group, they will still take this code path and receive the state
    // specified via BASE_FEATURE() above.
    return base::FeatureList::IsEnabled(kGlicZeroStateSuggestions);
  }

  return glic::GlicEnabling::IsInRolloutLocation();
#else
  return false;
#endif
}

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
    base::Hours(4));

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

const base::FeatureParam<bool> kUseDynamicCues(&kContextualCueing,
                                               "UseDynamicCues",
                                               false);

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

const base::FeatureParam<bool> kAllowContextualSuggestionsForSearchResultsPages(
    &kGlicZeroStateSuggestions,
    "ZSSAllowContextualSuggestionsForSearchResultsPages",
    true);

const base::FeatureParam<base::TimeDelta> kZSSPageContextTimeout(
    &kGlicZeroStateSuggestions,
    "ZSSPageContextTimeout",
    base::Seconds(5));

#if BUILDFLAG(ENABLE_GLIC)
const base::FeatureParam<int> kMaxPinnedPagesForTriggeringSuggestions(
    &glic::mojom::features::kZeroStateSuggestionsV2,
    "ZSSMaxPinnedPagesForTriggeringSuggestions",
    10);
#endif  // BUILDFLAG(ENABLE_GLIC)

}  // namespace contextual_cueing
