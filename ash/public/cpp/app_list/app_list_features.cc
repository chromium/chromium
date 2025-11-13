// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace app_list_features {

BASE_FEATURE(kEnableAppListLaunchRecording, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableExactMatchForNonLatinLocale,
             base::FEATURE_ENABLED_BY_DEFAULT);
// DO NOT REMOVE: Tast integration tests use this feature. (See crbug/1340267)
BASE_FEATURE(kForceShowContinueSection, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kDynamicSearchUpdateAnimation, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLauncherPlayStoreSearch, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAppsCollections, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsExactMatchForNonLatinLocaleEnabled() {
  return base::FeatureList::IsEnabled(kEnableExactMatchForNonLatinLocale);
}

bool IsAppListLaunchRecordingEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppListLaunchRecording);
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

bool IsAppsCollectionsEnabled() {
  return base::FeatureList::IsEnabled(kAppsCollections);
}

bool IsAppsCollectionsEnabledCounterfactually() {
  return IsAppsCollectionsEnabled() &&
         kAppsCollectionsEnabledCounterfactually.Get();
}

bool IsAppsCollectionsEnabledWithModifiedOrder() {
  return IsAppsCollectionsEnabled() &&
         kAppsCollectionsEnabledWithModifiedOrder.Get();
}

}  // namespace app_list_features
