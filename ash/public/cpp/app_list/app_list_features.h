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

// Enables app shortcuts search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppShortcutSearch;

// Enables background blur for the app list, lock screen, and tab switcher, also
// enables the AppsGridView mask layer. In this mode, slower devices may have
// choppier app list animations. crbug.com/765292.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableBackgroundBlur;

// Enables the Play Store app search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnablePlayStoreAppSearch;

// Enables in-app data search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppDataSearch;

// Enables the home launcher in tablet mode. In this mode, the launcher will be
// always shown right on top of the wallpaper. Home button will minimize all
// windows instead of toggling the launcher.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableHomeLauncher;

// Enables using gestures to show or hide the home launcher.
// TODO(crbug.com/872319): Remove this after the feature is launched.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableHomeLauncherGestures;

// Enables the Settings shortcut search.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableSettingsShortcutSearch;

// Enables the apps grid gap feature.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppsGridGapFeature;

// Enables the new style launcher (See details at http://crbug.com/857206).
ASH_PUBLIC_EXPORT extern const base::Feature kEnableNewStyleLauncher;

// Enables the feature to allow users to seamlessly continue reading a web page
// when they switch from phones or tablets to Chromebook.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableContinueReading;

// Enables the feature to display zero state suggestions.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableZeroStateSuggestions;

// Enables the feature to autocomplete text typed in the AppList search box.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppListSearchAutocomplete;

// Enables the feature to rank app search result using AppSearchResultRanker.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableAppSearchResultRanker;

bool ASH_PUBLIC_EXPORT IsAnswerCardEnabled();
bool ASH_PUBLIC_EXPORT IsAppShortcutSearchEnabled();
bool ASH_PUBLIC_EXPORT IsBackgroundBlurEnabled();
bool ASH_PUBLIC_EXPORT IsPlayStoreAppSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAppDataSearchEnabled();
bool ASH_PUBLIC_EXPORT IsHomeLauncherEnabled();
bool ASH_PUBLIC_EXPORT IsHomeLauncherGesturesEnabled();
bool ASH_PUBLIC_EXPORT IsSettingsShortcutSearchEnabled();
bool ASH_PUBLIC_EXPORT IsAppsGridGapFeatureEnabled();
bool ASH_PUBLIC_EXPORT IsNewStyleLauncherEnabled();
bool ASH_PUBLIC_EXPORT IsContinueReadingEnabled();
bool ASH_PUBLIC_EXPORT IsZeroStateSuggestionsEnabled();
bool ASH_PUBLIC_EXPORT IsAppListSearchAutocompleteEnabled();
bool ASH_PUBLIC_EXPORT IsAppSearchResultRankerEnabled();

std::string ASH_PUBLIC_EXPORT AnswerServerUrl();
std::string ASH_PUBLIC_EXPORT AnswerServerQuerySuffix();
std::string ASH_PUBLIC_EXPORT AppSearchResultRankerPredictorName();

}  // namespace app_list_features

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_FEATURES_H_
