// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "chromeos/components/libsegmentation/buildflags.h"
#include "chromeos/constants/chromeos_features.h"

namespace search_features {

BASE_FEATURE(kLauncherKeywordExtractionScoring,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearch, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherLocalImageSearchConfidence,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherLocalImageSearchRelevance,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchIca, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kICASupportedByHardware, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchOcr, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchIndexingLimit,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherImageSearchDebug,
             "kLauncherImageSearchDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherSearchFileScan,
             "kLauncherSearchFileScan",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLauncherKeyShortcutInBestMatch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(b/330386392): kLauncherGameSearch can be removed because if there's no
// payload, there will be no result.
bool IsLauncherGameSearchEnabled() {
  return chromeos::features::IsCloudGamingDeviceEnabled() ||
         chromeos::features::IsAlmanacLauncherPayloadEnabled();
}

bool IsLauncherKeywordExtractionScoringEnabled() {
  return base::FeatureList::IsEnabled(kLauncherKeywordExtractionScoring);
}

bool IsLauncherImageSearchEnabled() {
  return base::FeatureList::IsEnabled(
             ash::features::kFeatureManagementLocalImageSearch) &&
         base::FeatureList::IsEnabled(kLauncherImageSearch);
}

// Only enable ica image search for ICA supported devices.
bool IsLauncherImageSearchIcaEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchIca) &&
         base::FeatureList::IsEnabled(kICASupportedByHardware);
}

bool IsLauncherImageSearchOcrEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchOcr);
}

bool IsLauncherImageSearchIndexingLimitEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchIndexingLimit);
}

bool IsLauncherImageSearchDebugEnabled() {
  return base::FeatureList::IsEnabled(kLauncherImageSearchDebug);
}

bool IsLauncherSearchFileScanEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSearchFileScan);
}

bool IskLauncherKeyShortcutInBestMatchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherKeyShortcutInBestMatch);
}

}  // namespace search_features
