// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_

#include "base/feature_list.h"

namespace search_features {

// Enables cloud game search in the launcher.
BASE_DECLARE_FEATURE(kLauncherGameSearch);

// Query key word extraction and scoring while search in the launcher
BASE_DECLARE_FEATURE(kLauncherKeywordExtractionScoring);

// Federated analytics for launcher queries, via Private Heavy Hitters (PHH).
BASE_DECLARE_FEATURE(kLauncherQueryFederatedAnalyticsPHH);

// Change relevance score in Drive Files, Local Files, Help App, Keyboard
// shortcuts, OS Settings and personalization app to all be based on a fuzzy
// match
BASE_DECLARE_FEATURE(kLauncherFuzzyMatchAcrossProviders);

// Enables a fuzzy match between the query and title in Omnibox result to
// calculate the relevance
BASE_DECLARE_FEATURE(kLauncherFuzzyMatchForOmnibox);

// Enables image search in the launcher.
BASE_DECLARE_FEATURE(kLauncherImageSearch);

// Segmentation flag for local image search.
BASE_DECLARE_FEATURE(kFeatureManagementLocalImageSearch);

// Whether or not to override configuration of the local image search confidence
// threshold with an experiment.
BASE_DECLARE_FEATURE(kLauncherLocalImageSearchConfidence);

// Whether or not to override configuration of the local image search Relevance
// threshold with an experiment.
BASE_DECLARE_FEATURE(kLauncherLocalImageSearchRelevance);

// Enable Image Content-based Annotation
BASE_DECLARE_FEATURE(kLauncherImageSearchIca);

// Indicates whether Image Content-based Annotation is supported by hardware.
BASE_DECLARE_FEATURE(kICASupportedByHardware);

// Enable Optical Character Recognition
BASE_DECLARE_FEATURE(kLauncherImageSearchOcr);

// Applies a hard limit about how many images can be process per user session.
BASE_DECLARE_FEATURE(kLauncherImageSearchIndexingLimit);

BASE_DECLARE_FEATURE(kLauncherSystemInfoAnswerCards);

// Enable manatee for keyboard shortcuts
BASE_DECLARE_FEATURE(kLauncherManateeForKeyboardShortcuts);

bool IsLauncherGameSearchEnabled();
bool IsLauncherKeywordExtractionScoringEnabled();
bool IsLauncherQueryFederatedAnalyticsPHHEnabled();
bool IsLauncherImageSearchEnabled();
bool IsLauncherImageSearchIcaEnabled();
bool IsLauncherImageSearchOcrEnabled();
bool IsLauncherImageSearchIndexingLimitEnabled();
bool IsLauncherFuzzyMatchAcrossProvidersEnabled();
bool isLauncherFuzzyMatchForOmniboxEnabled();
bool isLauncherSystemInfoAnswerCardsEnabled();
bool isLauncherManateeForKeyboardShortcutsEnabled();

}  // namespace search_features

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_
