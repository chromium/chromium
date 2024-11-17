// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace app_list_features {

// Please keep these features sorted.
// TODO(newcomer|weidongg): Sort these features.

// Enables the feature to include a single reinstallation candidate in
// zero-state.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableAppReinstallZeroState);

// Enables hashed recording of a app list launches.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableAppListLaunchRecording);

// Enables using exact string search for non latin locales.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kEnableExactMatchForNonLatinLocale);

// Forces the launcher to show the continue section even if there are no file
// suggestions.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kForceShowContinueSection);

// Enable shortened search result update animations when in progress animations
// are interrupted by search model updates.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kDynamicSearchUpdateAnimation);

// Enables Play Store search in the launcher.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kLauncherPlayStoreSearch);

// Enables the Launcher Apps Collection UI in ChromeOS - a modular view with
// different categories filled with predefined apps.
ASH_PUBLIC_EXPORT BASE_DECLARE_FEATURE(kAppsCollections);

// Whether the Apps Collections is enabled counterfactually as part of an
// experiment arm.
const base::FeatureParam<bool> kAppsCollectionsEnabledCounterfactually{
    &kAppsCollections, "is-counterfactual", false};

// Whether the Apps Collections is enabled counterfactually as part of an
// experiment arm.
const base::FeatureParam<bool> kAppsCollectionsEnabledWithModifiedOrder{
    &kAppsCollections, "is-modified-order", false};

ASH_PUBLIC_EXPORT bool IsAppReinstallZeroStateEnabled();
ASH_PUBLIC_EXPORT bool IsAppListLaunchRecordingEnabled();
ASH_PUBLIC_EXPORT bool IsExactMatchForNonLatinLocaleEnabled();
ASH_PUBLIC_EXPORT bool IsForceShowContinueSectionEnabled();
ASH_PUBLIC_EXPORT bool IsAggregatedMlSearchRankingEnabled();
ASH_PUBLIC_EXPORT bool IsLauncherSearchNormalizationEnabled();
ASH_PUBLIC_EXPORT bool IsDynamicSearchUpdateAnimationEnabled();
ASH_PUBLIC_EXPORT base::TimeDelta DynamicSearchUpdateAnimationDuration();
ASH_PUBLIC_EXPORT bool IsLauncherPlayStoreSearchEnabled();
ASH_PUBLIC_EXPORT bool IsAppsCollectionsEnabled();
ASH_PUBLIC_EXPORT bool IsAppsCollectionsEnabledCounterfactually();
ASH_PUBLIC_EXPORT bool IsAppsCollectionsEnabledWithModifiedOrder();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
