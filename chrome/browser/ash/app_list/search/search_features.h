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

// Enables a fuzzy match between the query and title in Omnibox result to
// calculate the relevance
BASE_DECLARE_FEATURE(kLauncherFuzzyMatchForOmnibox);

// Enables image search in the launcher.
BASE_DECLARE_FEATURE(kLauncherImageSearch);

BASE_DECLARE_FEATURE(kLauncherSystemInfoAnswerCards);

bool IsLauncherGameSearchEnabled();
bool IsLauncherKeywordExtractionScoringEnabled();
bool IsLauncherQueryFederatedAnalyticsPHHEnabled();
bool IsLauncherImageSearchEnabled();
bool isLauncherFuzzyMatchForOmniboxEnabled();
bool isLauncherSystemInfoAnswerCardsEnabled();

}  // namespace search_features

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_FEATURES_H_
