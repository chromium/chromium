// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_

#include "base/feature_list.h"

namespace search_features {

// Enables cloud game search in the launcher.
BASE_DECLARE_FEATURE(kLauncherGameSearch);

// Query key word extraction and scoring while search in the launcher.
BASE_DECLARE_FEATURE(kLauncherKeywordExtractionScoring);

// Federated analytics for launcher queries, via Private Heavy Hitters (PHH).
BASE_DECLARE_FEATURE(kLauncherQueryFederatedAnalyticsPHH);

// Enables a fuzzy match between the query and title in Omnibox result to
// calculate the relevance.
BASE_DECLARE_FEATURE(kLauncherFuzzyMatchForOmnibox);

// Enables image search in the launcher.
BASE_DECLARE_FEATURE(kLauncherImageSearch);

// Whether or not to override configuration of the local image search confidence
// threshold with an experiment.
BASE_DECLARE_FEATURE(kLauncherLocalImageSearchConfidence);

// Whether or not to override configuration of the local image search Relevance
// threshold with an experiment.
BASE_DECLARE_FEATURE(kLauncherLocalImageSearchRelevance);

// Enable Image Content-based Annotation.
BASE_DECLARE_FEATURE(kLauncherImageSearchIca);

// Indicates whether Image Content-based Annotation is supported by hardware.
BASE_DECLARE_FEATURE(kICASupportedByHardware);

// Enable Optical Character Recognition.
BASE_DECLARE_FEATURE(kLauncherImageSearchOcr);

// Applies a hard limit about how many images can be process per user session.
BASE_DECLARE_FEATURE(kLauncherImageSearchIndexingLimit);

// Enable debugging for launcher image search. Currently it's only used in
// certain tast test and will introduce extra logs to help debug.
BASE_DECLARE_FEATURE(kLauncherImageSearchDebug);

BASE_DECLARE_FEATURE(kLauncherSystemInfoAnswerCards);

// Enables file scan in launcher. This is used as a stopper if the file scan ran
// into any issues.
BASE_DECLARE_FEATURE(kLauncherSearchFileScan);

// Allows keyboard shortcut results to appear in best match and answer card.
BASE_DECLARE_FEATURE(kLauncherKeyShortcutInBestMatch);

bool IsLauncherGameSearchEnabled();
bool IsLauncherKeywordExtractionScoringEnabled();
bool IsLauncherQueryFederatedAnalyticsPHHEnabled();
bool IsLauncherImageSearchEnabled();
bool IsLauncherImageSearchIcaEnabled();
bool IsLauncherImageSearchOcrEnabled();
bool IsLauncherImageSearchIndexingLimitEnabled();
bool IsLauncherImageSearchDebugEnabled();
bool IsLauncherFuzzyMatchForOmniboxEnabled();
bool IsLauncherSystemInfoAnswerCardsEnabled();
bool IsLauncherSearchFileScanEnabled();
bool IskLauncherKeyShortcutInBestMatchEnabled();
}  // namespace search_features

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_
