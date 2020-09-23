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

// Enables the answer card in the app list.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAnswerCard;

// Enables in-app data search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppDataSearch;

// Enables the Settings shortcut search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSettingsShortcutSearch;

// Enables the feature to display zero state suggestions.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateSuggestions;

// Enables the feature to autocomplete text typed in the AppList search box.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppListSearchAutocomplete;

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

// Enables file suggestions in the suggestion chips.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSuggestedFiles;

// Enables the Assistant search redirection in the app list.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAssistantSearch;

// Enables ghosting in any AppsGridView (folder or root) when dragging an item.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppGridGhost;

// Enables hashed recording of a app list launches.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppListLaunchRecording;

// Enables using the aggregated Ml model to rank suggested apps.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAggregatedMlAppRanking;

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

// Enables rich entity formatting for Omnibox results in the launcher.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableOmniboxRichEntities;

bool ASH_PUBLIC_EXPORT IsAnswerCardEnabled();
bool ASH_PUBLIC_EXPORT IsAppDataSearchEnabled();
bool ASH_PUBLIC_EXPORT IsSettingsShortcutSearchEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateSuggestionsEnabled();
bool ASH_PUBLIC_EXPORT IsAppListSearchAutocompleteEnabled();
bool ASH_PUBLIC_EXPORT IsAppRankerEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateAppsRankerEnabled();
bool ASH_PUBLIC_EXPORT IsQueryBasedMixedTypesRankerEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateMixedTypesRankerEnabled();
bool ASH_PUBLIC_EXPORT IsAppReinstallZeroStateEnabled();
bool ASH_PUBLIC_EXPORT IsSuggestedFilesEnabled();
bool ASH_PUBLIC_EXPORT IsAssistantSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAppGridGhostEnabled();
bool ASH_PUBLIC_EXPORT IsAppListLaunchRecordingEnabled();
bool ASH_PUBLIC_EXPORT IsAggregatedMlAppRankingEnabled();
bool ASH_PUBLIC_EXPORT IsFuzzyAppSearchEnabled();
bool ASH_PUBLIC_EXPORT IsExactMatchForNonLatinLocaleEnabled();
bool ASH_PUBLIC_EXPORT IsLauncherSettingsSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAggregatedMlSearchRankingEnabled();
bool ASH_PUBLIC_EXPORT IsNewDragSpecInLauncherEnabled();
bool ASH_PUBLIC_EXPORT IsOmniboxRichEntitiesEnabled();

std::string ASH_PUBLIC_EXPORT AnswerServerUrl();
std::string ASH_PUBLIC_EXPORT AnswerServerQuerySuffix();
std::string ASH_PUBLIC_EXPORT AppSearchResultRankerPredictorName();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
