// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_features.h"

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chromeos/constants/chromeos_features.h"

namespace app_list_features {

const base::Feature kEnableAppDataSearch{"EnableAppDataSearch",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableSettingsShortcutSearch{
    "EnableSettingsShortcutSearch", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAppListSearchAutocomplete{
    "EnableAppListSearchAutocomplete", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableAppRanker{"EnableAppRanker",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableZeroStateAppsRanker{
    "EnableZeroStateAppsRanker", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableQueryBasedMixedTypesRanker{
    "EnableQueryBasedMixedTypesRanker", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableZeroStateMixedTypesRanker{
    "EnableZeroStateMixedTypesRanker", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableAppReinstallZeroState{
    "EnableAppReinstallZeroState", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableSuggestedFiles{"EnableSuggestedFiles",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// "EnableEmbeddedAssistantUI" is used in finch experiment therefore we cannot
// change it until fully launched. It is used to redirect Launcher search to
// Assistant search.
const base::Feature kEnableAssistantSearch{"EnableEmbeddedAssistantUI",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableAppGridGhost{"EnableAppGridGhost",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableAppListLaunchRecording{
    "EnableAppListLaunchRecording", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kLauncherSettingsSearch{"LauncherSettingsSearch",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableFuzzyAppSearch{"EnableFuzzyAppSearch",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableExactMatchForNonLatinLocale{
    "EnableExactMatchForNonLatinLocale", base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableAggregatedMlSearchRanking{
    "EnableAggregatedMlSearchRanking", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNewDragSpecInLauncher{"NewDragSpecInLauncher",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kEnableOmniboxRichEntities{
    "EnableOmniboxRichEntities", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableLauncherSearchNormalization{
    "EnableLauncherSearchNormalization", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsAppDataSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppDataSearch);
}

bool IsSettingsShortcutSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableSettingsShortcutSearch);
}

bool IsAppListSearchAutocompleteEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppListSearchAutocomplete);
}

bool IsAppRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppRanker);
}

bool IsZeroStateAppsRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableZeroStateAppsRanker);
}

bool IsQueryBasedMixedTypesRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableQueryBasedMixedTypesRanker);
}

bool IsZeroStateMixedTypesRankerEnabled() {
  return base::FeatureList::IsEnabled(kEnableZeroStateMixedTypesRanker);
}

bool IsAppReinstallZeroStateEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppReinstallZeroState);
}

bool IsSuggestedFilesEnabled() {
  return base::FeatureList::IsEnabled(kEnableSuggestedFiles);
}

bool IsAssistantSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableAssistantSearch);
}

bool IsAppGridGhostEnabled() {
  return base::FeatureList::IsEnabled(kEnableAppGridGhost);
}

bool IsLauncherSettingsSearchEnabled() {
  return base::FeatureList::IsEnabled(kLauncherSettingsSearch);
}

bool IsFuzzyAppSearchEnabled() {
  return base::FeatureList::IsEnabled(kEnableFuzzyAppSearch);
}

bool IsExactMatchForNonLatinLocaleEnabled() {
  return base::FeatureList::IsEnabled(kEnableExactMatchForNonLatinLocale);
}

bool IsAggregatedMlSearchRankingEnabled() {
  return base::FeatureList::IsEnabled(kEnableAggregatedMlSearchRanking);
}

bool IsNewDragSpecInLauncherEnabled() {
  return base::FeatureList::IsEnabled(kNewDragSpecInLauncher);
}

bool IsOmniboxRichEntitiesEnabled() {
  return base::FeatureList::IsEnabled(kEnableOmniboxRichEntities);
}

bool IsLauncherSearchNormalizationEnabled() {
  return base::FeatureList::IsEnabled(kEnableLauncherSearchNormalization);
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

}  // namespace app_list_features
