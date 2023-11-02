// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace app_list_features {

BASE_FEATURE(kEnableAppRanker,
             "EnableAppRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableZeroStateAppsRanker,
             "EnableZeroStateAppsRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableZeroStateMixedTypesRanker,
             "EnableZeroStateMixedTypesRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAppReinstallZeroState,
             "EnableAppReinstallZeroState",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAppListLaunchRecording,
             "EnableAppListLaunchRecording",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableExactMatchForNonLatinLocale,
             "EnableExactMatchForNonLatinLocale",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCategoricalSearch,
             "CategoricalSearch",
             base::FEATURE_DISABLED_BY_DEFAULT);
// DO NOT REMOVE: Tast integration tests use this feature. (See crbug/1340267)
BASE_FEATURE(kForceShowContinueSection,
             "ForceShowContinueSection",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchResultInlineIcon,
             "SearchResultInlineIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kQuickActionShowBubbleLauncher,
             "QuickActionShowBubbleLauncher",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDynamicSearchUpdateAnimation,
             "DynamicSearchUpdateAnimation",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCompactBubbleLauncher,
             "CompactBubbleLauncher",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLauncherPlayStoreSearch,
             "LauncherPlayStoreSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsAppRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppRanker);
}

bool IsZeroStateAppsRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableZeroStateAppsRanker);
}

bool IsZeroStateMixedTypesRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableZeroStateMixedTypesRanker);
}

bool IsAppReinstallZeroStateEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppReinstallZeroState);
}

bool IsExactMatchForNonLatinLocaleEnabled() {
  return base::FeatureList::IsEnabled(kEnableExactMatchForNonLatinLocale);
}

std::string AppSearchResultRankerPredictorName() {
  const std::string predictor_name = base::GetFieldTrialParamValueByFeature(
      kEnableZeroStateAppsRanker, "app_search_result_ranker_predictor_name");
  if (!predictor_name.empty())
    return predictor_name;
  return std::string("MrfuAppLaunchPredictor");
}

bool IsAppListLaunchRecordingEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppListLaunchRecording);
}

bool IsCategoricalSearchEnabled() {
  // Force categorical search for the latest version of the launcher.
  return ash::features::IsProductivityLauncherEnabled() ||
         base::FeatureList::IsEnabled(kCategoricalSearch);
}

bool IsSearchResultInlineIconEnabled() {
  // Inline Icons are only supported for categorical search.
  return IsCategoricalSearchEnabled() &&
         base::FeatureList::IsEnabled(kSearchResultInlineIcon);
}

bool IsQuickActionShowBubbleLauncherEnabled() {
  return ash::features::IsProductivityLauncherEnabled() &&
         base::FeatureList::IsEnabled(kQuickActionShowBubbleLauncher);
}

bool IsDynamicSearchUpdateAnimationEnabled() {
  // Search update animations are only supported for categorical search.
  return IsCategoricalSearchEnabled() &&
         base::FeatureList::IsEnabled(kDynamicSearchUpdateAnimation);
}

std::string CategoricalSearchType() {
  return GetFieldTrialParamValueByFeature(kCategoricalSearch, "ranking");
}

base::TimeDelta DynamicSearchUpdateAnimationDuration() {
  int ms = base::GetFieldTrialParamByFeatureAsInt(
      kDynamicSearchUpdateAnimation, "animation_time", /*default value =*/100);
  return base::TimeDelta(base::Milliseconds(ms));
}

bool IsForceShowContinueSectionEnabled() {
  return base::FeatureList::IsEnabled(kForceShowContinueSection);
}

bool IsCompactBubbleLauncherEnabled() {
  return base::FeatureList::IsEnabled(kCompactBubbleLauncher);
}

bool IsLauncherPlayStoreSearchEnabled() {
  return ash::features::IsProductivityLauncherEnabled() &&
         base::FeatureList::IsEnabled(kLauncherPlayStoreSearch);
}

}  // namespace app_list_features
