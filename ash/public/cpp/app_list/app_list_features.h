// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace base {
struct Feature;
}

namespace app_list_features {

// Please keep these features sorted.
// TODO(newcomer|weidongg): Sort these features.

// Enable app ranking models.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppRanker;

// Enable a model that ranks zero-state apps search result.
// TODO(crbug.com/989350): This flag can be removed once the
// AppSearchResultRanker is removed. Same with the
// AppSearchResultRankerPredictorName.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateAppsRanker;

// Enable a model that ranks query based non-apps result.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableQueryBasedMixedTypesRanker;

// Enable a model that ranks zero-state files and recent queries.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateMixedTypesRanker;

// Enables the feature to include a single reinstallation candidate in
// zero-state.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppReinstallZeroState;

// Enables Drive file suggestions in the suggestion chips.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSuggestedFiles;

// Enables local file suggestions in the suggestion chips.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSuggestedLocalFiles;

// Enables the Assistant search redirection in the app list.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAssistantSearch;

// Enables hashed recording of a app list launches.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppListLaunchRecording;

// Enables using the fuzzy search algorithm for app search provider.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableFuzzyAppSearch;

// Enables using exact string search for non latin locales.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableExactMatchForNonLatinLocale;

// Enables launcher search results for OS settings.
ASH_PUBLIC_EXPORT extern const base::Feature kLauncherSettingsSearch;

// Enables using aggregated model in ranking non-app results for
// non empty queries.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAggregatedMlSearchRanking;

// Enables the new app dragging in the launcher. When the users drags an app
// within the launcher, this flag will enable the new cardified state, where
// apps grid pages are scaled down and shown a background card.
ASH_PUBLIC_EXPORT extern const base::Feature kNewDragSpecInLauncher;

// Enables normalization of search results in the launcher.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableLauncherSearchNormalization;

// Enables categorical search in the launcher.
ASH_PUBLIC_EXPORT extern const base::Feature kCategoricalSearch;

// Forces the launcher to show the continue section even if there are no file
// suggestions.
ASH_PUBLIC_EXPORT extern const base::Feature kForceShowContinueSection;

bool ASH_PUBLIC_EXPORT IsAppRankerEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateAppsRankerEnabled();
bool ASH_PUBLIC_EXPORT IsQueryBasedMixedTypesRankerEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateMixedTypesRankerEnabled();
bool ASH_PUBLIC_EXPORT IsAppReinstallZeroStateEnabled();
bool ASH_PUBLIC_EXPORT IsSuggestedFilesEnabled();
bool ASH_PUBLIC_EXPORT IsSuggestedLocalFilesEnabled();
bool ASH_PUBLIC_EXPORT IsAssistantSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAppListLaunchRecordingEnabled();
bool ASH_PUBLIC_EXPORT IsFuzzyAppSearchEnabled();
bool ASH_PUBLIC_EXPORT IsExactMatchForNonLatinLocaleEnabled();
bool ASH_PUBLIC_EXPORT IsForceShowContinueSectionEnabled();
bool ASH_PUBLIC_EXPORT IsLauncherSettingsSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAggregatedMlSearchRankingEnabled();
bool ASH_PUBLIC_EXPORT IsNewDragSpecInLauncherEnabled();
bool ASH_PUBLIC_EXPORT IsLauncherSearchNormalizationEnabled();
bool ASH_PUBLIC_EXPORT IsCategoricalSearchEnabled();

std::string ASH_PUBLIC_EXPORT AnswerServerUrl();
std::string ASH_PUBLIC_EXPORT AnswerServerQuerySuffix();
std::string ASH_PUBLIC_EXPORT AppSearchResultRankerPredictorName();
std::string ASH_PUBLIC_EXPORT CategoricalSearchType();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
