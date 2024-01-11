// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_features.h"

#include "base/feature_list.h"
#include "chromeos/constants/chromeos_features.h"

namespace search_features {

BASE_FEATURE(kLauncherGameSearch,
             "LauncherGameSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherKeywordExtractionScoring,
             "LauncherKeywordExtractionScoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherQueryFederatedAnalyticsPHH,
             "LauncherQueryFederatedAnalyticsPHH",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherFuzzyMatchAcrossProviders,
             "LauncherFuzzyMatchAcrossProviders",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherFuzzyMatchForOmnibox,
             "LauncherFuzzyMatchForOmnibox",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearch,
             "LauncherImageSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchIca,
             "LauncherImageSearchIca",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kICASupportedByHardware,
             "ICASupportedByHardware",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchOcr,
             "LauncherImageSearchOcr",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherSystemInfoAnswerCards,
             "LauncherSystemInfoAnswerCards",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherManateeForKeyboardShortcuts,
             "LauncherManateeForKeyboardShortcuts",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLauncherGameSearchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherGameSearch) ||
         chromeos::features::IsCloudGamingDeviceEnabled();
}

bool IsLauncherKeywordExtractionScoringEnabled() {
  return base::FeatureList::IsEnabled(kLauncherKeywordExtractionScoring);
}

bool IsLauncherQueryFederatedAnalyticsPHHEnabled() {
  return base::FeatureList::IsEnabled(kLauncherQueryFederatedAnalyticsPHH);
}

bool IsLauncherFuzzyMatchAcrossProvidersEnabled() {
  return base::FeatureList::IsEnabled(kLauncherFuzzyMatchAcrossProviders);
}

bool isLauncherFuzzyMatchForOmniboxEnabled() {
  return base::FeatureList::IsEnabled(kLauncherFuzzyMatchForOmnibox);
}

// Only enable image search for ICA supported devices.
bool IsLauncherImageSearchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearch) &&
         base::FeatureList::IsEnabled(kICASupportedByHardware);
}

bool IsLauncherImageSearchIcaEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchIca);
}

bool IsLauncherImageSearchOcrEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchOcr);
}

bool isLauncherSystemInfoAnswerCardsEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSystemInfoAnswerCards);
}

bool isLauncherManateeForKeyboardShortcutsEnabled() {
  return base::FeatureList::IsEnabled(kLauncherManateeForKeyboardShortcuts);
}

}  // namespace search_features
