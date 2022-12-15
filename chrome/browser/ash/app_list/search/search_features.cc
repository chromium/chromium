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

bool IsLauncherGameSearchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherGameSearch) ||
         chromeos::features::IsCloudGamingDeviceEnabled();
}

BASE_FEATURE(kLauncherKeywordExtractionScoring,
             "LauncherKeywordExtractionScoring",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLauncherKeywordExtractionScoringEnabled() {
  return base::FeatureList::IsEnabled(kLauncherKeywordExtractionScoring);
}

BASE_FEATURE(kLauncherQueryFederatedAnalyticsPHH,
             "LauncherQueryFederatedAnalyticsPHH",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsLauncherQueryFederatedAnalyticsPHHEnabled() {
  return base::FeatureList::IsEnabled(kLauncherQueryFederatedAnalyticsPHH);
}

}  // namespace search_features
