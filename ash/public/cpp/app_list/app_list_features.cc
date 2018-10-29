// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace app_list_features {

const base::Feature kEnableAnswerCard{"EnableAnswerCard",
                                      base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableAppShortcutSearch{"EnableAppShortcutSearch",
                                             base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableBackgroundBlur{"EnableBackgroundBlur",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnablePlayStoreAppSearch{
    "EnablePlayStoreAppSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAppDataSearch{"EnableAppDataSearch",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableHomeLauncher{"EnableHomeLauncher",
                                        base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableHomeLauncherGestures{
    "HomeLauncherGestures", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableSettingsShortcutSearch{
    "EnableSettingsShortcutSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAppsGridGapFeature{"EnableAppsGridGapFeature",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableNewStyleLauncher{"EnableNewStyleLauncher",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableContinueReading{"EnableContinueReading",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableZeroStateSuggestions{
    "EnableZeroStateSuggestions", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAppListSearchAutocomplete{
    "EnableAppListSearchAutocomplete", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableAppSearchResultRanker{
    "EnableAppSearchResultRanker", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAnswerCardEnabled() {
  // Not using local static variable to allow tests to change this value.
  return base::FeatureList::IsEnabled(kEnableAnswerCard);
}

bool IsAppShortcutSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppShortcutSearch);
}

bool IsBackgroundBlurEnabled() {
  return base::FeatureList::IsEnabled(kEnableBackgroundBlur);
}

bool IsPlayStoreAppSearchEnabled() {
  // Not using local static variable to allow tests to change this value.
  return base::FeatureList::IsEnabled(kEnablePlayStoreAppSearch);
}

bool IsAppDataSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppDataSearch);
}

bool IsHomeLauncherEnabled() {
  return base::FeatureList::IsEnabled(kEnableHomeLauncher);
}

bool IsHomeLauncherGesturesEnabled() {
  return base::FeatureList::IsEnabled(kEnableHomeLauncherGestures);
}

bool IsSettingsShortcutSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableSettingsShortcutSearch);
}

bool IsAppsGridGapFeatureEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppsGridGapFeature);
}

bool IsNewStyleLauncherEnabled() {
  return base::FeatureList::IsEnabled(kEnableNewStyleLauncher);
}

bool IsContinueReadingEnabled() {
  return base::FeatureList::IsEnabled(kEnableContinueReading);
}

bool IsZeroStateSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(kEnableZeroStateSuggestions);
}

bool IsAppListSearchAutocompleteEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppListSearchAutocomplete);
}

bool IsAppSearchResultRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppSearchResultRanker);
}

std::string AnswerServerUrl() {
  const std::string experiment_url =
      base::GetFieldTrialParamValueByFeature(kEnableAnswerCard, "ServerUrl");
  if (!experiment_url.empty())
    return experiment_url;
  return "https://www.google.com/coac";
}

std::string AnswerServerQuerySuffix() {
  return base::GetFieldTrialParamValueByFeature(kEnableAnswerCard,
                                                "QuerySuffix");
}

std::string AppSearchResultRankerPredictorName() {
  const std::string predictor_name = base::GetFieldTrialParamValueByFeature(
      kEnableAppSearchResultRanker, "app_search_result_ranker_predictor_name");
  if (!predictor_name.empty())
    return predictor_name;
  return std::string("MrfuAppLaunchPredictor");
}

}  // namespace app_list_features
