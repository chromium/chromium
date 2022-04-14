// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"

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

// Enable a model that ranks zero-state files and recent queries.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateMixedTypesRanker;

// Enables the feature to include a single reinstallation candidate in
// zero-state.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppReinstallZeroState;

// Enables Drive file suggestions in the suggestion chips.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSuggestedFiles;

// Enables local file suggestions in the suggestion chips.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSuggestedLocalFiles;

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

// Enables normalization of search results in the launcher.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableLauncherSearchNormalization;

// Enables categorical search in the launcher.
ASH_PUBLIC_EXPORT extern const base::Feature kCategoricalSearch;

// Forces the launcher to show the continue section even if there are no file
// suggestions.
ASH_PUBLIC_EXPORT extern const base::Feature kForceShowContinueSection;

// Enables iconified text and inline icons in launcher search.
ASH_PUBLIC_EXPORT extern const base::Feature kSearchResultInlineIcon;

// Enable shortened search result update animations when in progress animations
// are interrupted by search model updates.
ASH_PUBLIC_EXPORT extern const base::Feature kDynamicSearchUpdateAnimation;

// Forces the Launcher provider to send Omnibox search queries to the lacros
// browser. If disabled, queries are sent to the ash browser.
ASH_PUBLIC_EXPORT extern const base::Feature kLauncherLacrosIntegration;

// Shows a feedback dialog when removing a continue section suggestion.
ASH_PUBLIC_EXPORT extern const base::Feature kFeedbackOnContinueSectionRemove;

// Controls the bubble launcher (productivity launcher in clamshell) width. When
// enabled, the bubble UI will be narrower.
ASH_PUBLIC_EXPORT extern const base::Feature kCompactBubbleLauncher;

ASH_PUBLIC_EXPORT bool IsAppRankerEnabled();
ASH_PUBLIC_EXPORT bool IsZeroStateAppsRankerEnabled();
ASH_PUBLIC_EXPORT bool IsQueryBasedMixedTypesRankerEnabled();
ASH_PUBLIC_EXPORT bool IsZeroStateMixedTypesRankerEnabled();
ASH_PUBLIC_EXPORT bool IsAppReinstallZeroStateEnabled();
ASH_PUBLIC_EXPORT bool IsSuggestedFilesEnabled();
ASH_PUBLIC_EXPORT bool IsSuggestedLocalFilesEnabled();
ASH_PUBLIC_EXPORT bool IsAppListLaunchRecordingEnabled();
ASH_PUBLIC_EXPORT bool IsFuzzyAppSearchEnabled();
ASH_PUBLIC_EXPORT bool IsExactMatchForNonLatinLocaleEnabled();
ASH_PUBLIC_EXPORT bool IsForceShowContinueSectionEnabled();
ASH_PUBLIC_EXPORT bool IsLauncherSettingsSearchEnabled();
ASH_PUBLIC_EXPORT bool IsAggregatedMlSearchRankingEnabled();
ASH_PUBLIC_EXPORT bool IsLauncherSearchNormalizationEnabled();
ASH_PUBLIC_EXPORT bool IsCategoricalSearchEnabled();
ASH_PUBLIC_EXPORT bool IsSearchResultInlineIconEnabled();
ASH_PUBLIC_EXPORT bool IsDynamicSearchUpdateAnimationEnabled();
ASH_PUBLIC_EXPORT base::TimeDelta DynamicSearchUpdateAnimationDuration();
ASH_PUBLIC_EXPORT bool IsLauncherLacrosIntegrationEnabled();
ASH_PUBLIC_EXPORT bool IsFeedbackOnContinueSectionRemoveEnabled();
ASH_PUBLIC_EXPORT bool IsCompactBubbleLauncherEnabled();

ASH_PUBLIC_EXPORT std::string AnswerServerUrl();
ASH_PUBLIC_EXPORT std::string AnswerServerQuerySuffix();
ASH_PUBLIC_EXPORT std::string AppSearchResultRankerPredictorName();
ASH_PUBLIC_EXPORT std::string CategoricalSearchType();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
