// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_FEATURES_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace contextual_cueing {

BASE_DECLARE_FEATURE(kContextualCueing);
BASE_DECLARE_FEATURE(kGlicZeroStateSuggestions);

// Whether contextual cueing is enabled.
//
// It is expected for downstream to use this function rather than calling
// `base::FeatureList::IsEnabled` directly. Note that this function explicitly
// ignores country/locale if the feature is overridden by Finch or command-line
// override.
bool IsContextualCueingEnabled();

// Whether zero state suggestions are enabled.
//
// It is expected for downstream to use this function rather than calling
// `base::FeatureList::IsEnabled` directly. Note that this function explicitly
// ignores country/locale if the feature is overridden by Finch or command-line
// override.
bool IsZeroStateSuggestionsEnabled();

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

// The same hard cap limits per domain. ie., x nudges per y hours per domain.
extern const base::FeatureParam<base::TimeDelta> kNudgeCapTimePerDomain;
extern const base::FeatureParam<int> kNudgeCapCountPerDomain;

// A hard limit of page navigation counts between nudges. After one nudge is
// shown, there must be at least `kMinPageCountBetweenNudges` page navigations
// before the next nudge can be shown.
extern const base::FeatureParam<int> kMinPageCountBetweenNudges;

// The minimum time between two consecutive nudges. Prevents excessive nudges
// during a short burst of navigations.
extern const base::FeatureParam<base::TimeDelta> kMinTimeBetweenNudges;

// Limit on how many recently visited domains should be kept track of. This is
// used to implement nudge constraints per-domain per 24 hour period.
extern const base::FeatureParam<int> kVisitedDomainsLimit;

// The amount of time to wait for capturing the page count for a PDF document.
extern const base::FeatureParam<base::TimeDelta> kPdfPageCountCaptureDelay;

// Whether to enable page content extraction which is needed for processing the
// count of words client signal.
extern const base::FeatureParam<bool> kEnablePageContentExtraction;

// Whether or not to use static or dynamic cue texts based on the page.
extern const base::FeatureParam<bool> kUseDynamicCues;

// Whether to enable extraction of inner text for zero state suggestions.
extern const base::FeatureParam<bool> kExtractInnerTextForZeroStateSuggestions;

// Whether to enable extraction of annotated page content for zero state
// suggestions.
extern const base::FeatureParam<bool>
    kExtractAnnotatedPageContentForZeroStateSuggestions;

// The amount of time to wait for extracting page content for same document
// navigations.
extern const base::FeatureParam<base::TimeDelta>
    kPageContentExtractionDelayForSameDocumentNavigation;

// Always return empty suggestions for same document navigations.
extern const base::FeatureParam<bool> kReturnEmptyForSameDocumentNavigation;

// Whether to allow contextual zero state suggestions for search results pages.
extern const base::FeatureParam<bool>
    kAllowContextualSuggestionsForSearchResultsPages;

// How many pages can be pinned and still trigger a zero state suggestion.
extern const base::FeatureParam<int> kMaxPinnedPagesForTriggeringSuggestions;

// Timeout before giving up on getting context from a page.
extern const base::FeatureParam<base::TimeDelta> kZSSPageContextTimeout;
}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_FEATURES_H_
