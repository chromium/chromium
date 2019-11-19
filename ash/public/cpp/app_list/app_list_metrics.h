// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_METRICS_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_METRICS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
enum class AppListLaunchedFrom;

// The type of the ChromeSearchResult. This is used for logging so do not
// change the order of this enum. If you add to this enum update
// AppListSearchResult in enums.xml.
enum SearchResultType {
  // A result that forwards an omnibox search result. Use or create OMNIBOX_*
  // instead (Deprecated).
  OMNIBOX_SEARCH_RESULT_DEPRECATED,
  // An app result. Use or create platform specific types below (Deprecated).
  APP_SEARCH_RESULT_DEPRECATED,
  // A search result from the webstore (Deprecated).
  WEBSTORE_SEARCH_RESULT_DEPRECATED,
  // A result that opens a webstore search (Deprecated)
  SEARCH_WEBSTORE_SEARCH_RESULT_DEPRECATED,
  // A result that opens a people search (Deprecated).
  SEARCH_PEOPLE_SEARCH_RESULT_DEPRECATED,
  // A result that opens a suggestion (Deprecated).
  SUGGESTIONS_SEARCH_RESULT_DEPRECATED,
  // A result which is either a local file or drive file.
  LAUNCHER_SEARCH_PROVIDER_RESULT,
  // A result that is an uninstalled app from a Play Store app search.
  PLAY_STORE_UNINSTALLED_APP,
  // A result that is an instant app from a Play Store app search.
  PLAY_STORE_INSTANT_APP,
  // A result that is an answer card.
  ANSWER_CARD,
  // A result that opens a specific activity in an app installed from Play
  // Store.
  PLAY_STORE_APP_SHORTCUT,
  // A result that is a URL.
  OMNIBOX_URL_WHAT_YOU_TYPED,
  // A result which is a bookmark.
  OMNIBOX_BOOKMARK,
  // A result which is a recently visited website.
  OMNIBOX_RECENTLY_VISITED_WEBSITE,
  // A result which is a recently used doc in drive.
  OMNIBOX_RECENT_DOC_IN_DRIVE,
  // A result which is a web query.
  OMNIBOX_WEB_QUERY,
  // A result which was a web query that was previously searched.
  // This should be deprecated after M76.
  OMNIBOX_HISTORY_DEPRECATED,
  // An app result which is an installed playstore app.
  PLAY_STORE_APP,
  // An app result which is an app that was installed on another device.
  PLAY_STORE_REINSTALL_APP,
  // An app result which is an internal app (files, settings, etc).
  INTERNAL_APP,
  // An app result which is an extension.
  EXTENSION_APP,
  // A Crostini App Result.
  CROSTINI_APP,
  // An app result which is a quick action in settings.
  SETTINGS_SHORTCUT,
  // An ArcAppDataSearchResult which is a person from contacts.
  APP_DATA_RESULT_PERSON,
  // An ArcAppDataSearchResult which is a note document.
  APP_DATA_RESULT_NOTE_DOCUMENT,
  // An omnibox result which is opened via the assistant.
  ASSISTANT_OMNIBOX_RESULT,
  // A result from omnibox for the query that was previously searched.
  OMNIBOX_SEARCH_HISTORY,
  // A result from omnibox for query suggestion.
  OMNIBOX_SEARCH_SUGGEST,
  // A result from omnibox for the personalized suggestion.
  // Currently, it is used for the user's recent query.
  OMNIBOX_SUGGEST_PERSONALIZED,
  // A zero-state result representing a local file.
  ZERO_STATE_FILE,
  // A result from the Drive QuickAccess provider.
  DRIVE_QUICK_ACCESS,
  // Boundary is always last.
  SEARCH_RESULT_TYPE_BOUNDARY
};

ASH_PUBLIC_EXPORT void RecordSearchResultOpenTypeHistogram(
    ash::AppListLaunchedFrom launch_location,
    SearchResultType type,
    bool is_tablet_mode);

ASH_PUBLIC_EXPORT void RecordDefaultSearchResultOpenTypeHistogram(
    SearchResultType type);

ASH_PUBLIC_EXPORT void RecordZeroStateSuggestionOpenTypeHistogram(
    SearchResultType type);

ASH_PUBLIC_EXPORT void RecordLauncherIssuedSearchQueryLength(int query_length);

ASH_PUBLIC_EXPORT void RecordSuccessfulAppLaunchUsingSearch(
    ash::AppListLaunchedFrom launched_from,
    int query_length);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_METRICS_H_
