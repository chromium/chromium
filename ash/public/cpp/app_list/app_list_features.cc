// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace app_list_features {

BASE_FEATURE(kEnableAppReinstallZeroState,
             "EnableAppReinstallZeroState",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableAppListLaunchRecording,
             "EnableAppListLaunchRecording",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableExactMatchForNonLatinLocale,
             "EnableExactMatchForNonLatinLocale",
             base::FEATURE_ENABLED_BY_DEFAULT);
// DO NOT REMOVE: Tast integration tests use this feature. (See crbug/1340267)
BASE_FEATURE(kForceShowContinueSection,
             "ForceShowContinueSection",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSearchResultInlineIcon,
             "SearchResultInlineIcon",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kQuickActionShowBubbleLauncher,
             "QuickActionShowBubbleLauncher",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDynamicSearchUpdateAnimation,
             "DynamicSearchUpdateAnimation",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLauncherPlayStoreSearch,
             "LauncherPlayStoreSearch",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDragAndDropRefactor,
             "AppListDragAndDropRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAppReinstallZeroStateEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppReinstallZeroState);
}

bool IsExactMatchForNonLatinLocaleEnabled() {
  return base::FeatureList::IsEnabled(kEnableExactMatchForNonLatinLocale);
}

bool IsAppListLaunchRecordingEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppListLaunchRecording);
}

bool IsSearchResultInlineIconEnabled() {
  // Inline Icons are only supported for categorical search.
  return base::FeatureList::IsEnabled(kSearchResultInlineIcon);
}

bool IsQuickActionShowBubbleLauncherEnabled() {
  return base::FeatureList::IsEnabled(kQuickActionShowBubbleLauncher);
}

bool IsDynamicSearchUpdateAnimationEnabled() {
  // Search update animations are only supported for categorical search.
  return base::FeatureList::IsEnabled(kDynamicSearchUpdateAnimation);
}

base::TimeDelta DynamicSearchUpdateAnimationDuration() {
  int ms = base::GetFieldTrialParamByFeatureAsInt(
      kDynamicSearchUpdateAnimation, "animation_time", /*default value =*/100);
  return base::TimeDelta(base::Milliseconds(ms));
}

bool IsForceShowContinueSectionEnabled() {
  return base::FeatureList::IsEnabled(kForceShowContinueSection);
}

bool IsLauncherPlayStoreSearchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherPlayStoreSearch);
}

bool IsDragAndDropRefactorEnabled() {
  return base::FeatureList::IsEnabled(kDragAndDropRefactor);
}

}  // namespace app_list_features
