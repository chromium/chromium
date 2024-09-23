// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_METRICS_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_METRICS_H_

#include "ash/public/cpp/ash_public_export.h"

#include <string>

namespace ash {
enum class AppListLaunchedFrom;
enum class AppListOrderUpdateEvent;
enum class AppListSortOrder;

// UMA histograms that record the actions that clear the pref sort order.
ASH_PUBLIC_EXPORT extern const char kClamshellPrefOrderClearActionHistogram[];
ASH_PUBLIC_EXPORT extern const char kTabletPrefOrderClearActionHistogram[];

// UMA histograms that record app list pref sort order when a session starts.
// Exposed in this header because they are needed in tests.
ASH_PUBLIC_EXPORT extern const char
    kClamshellAppListSortOrderOnSessionStartHistogram[];
ASH_PUBLIC_EXPORT extern const char
    kTabletAppListSortOrderOnSessionStartHistogram[];

// The UMA histogram that records the time duration between the app list sort
// education nudge show and the first sort usage.
ASH_PUBLIC_EXPORT extern const char kAppListSortDiscoveryDurationAfterNudge[];

// Similar to `kAppListSortDiscoveryDurationAfterNudge`. The only difference is
// that the metric data is separated by the tablet mode state under which the
// reorder education nudge shows.
ASH_PUBLIC_EXPORT extern const char
    kAppListSortDiscoveryDurationAfterNudgeClamshell[];
ASH_PUBLIC_EXPORT extern const char
    kAppListSortDiscoveryDurationAfterNudgeTablet[];

// The UMA histogram that records the time duration between the earliest user
// session activation with the app list sort enabled and the first sort usage.
ASH_PUBLIC_EXPORT extern const char
    kAppListSortDiscoveryDurationAfterActivation[];

// The different ways the app list can be shown. These values are written to
// logs.  New enum values can be added, but existing enums must never be
// renumbered or deleted and reused.
enum class AppListShowSource {
  kSearchKey = 0,
  kShelfButton = 1,
  kSwipeFromShelf = 2,
  kTabletMode = 3,
  kSearchKeyFullscreen_DEPRECATED = 4,    // Migrated to kSearchKey.
  kShelfButtonFullscreen_DEPRECATED = 5,  // Obsolete on bubble launcher.
  kAssistantEntryPoint = 6,
  kScrollFromShelf = 7,
  kBrowser = 8,
  kWelcomeTour = 9,
  kMaxValue = kWelcomeTour,
};

// Tracks the conclusion of each search session starting from the search box.
enum class SearchSessionConclusion {
  kQuit = 0,
  kLaunch = 1,
  kAnswerCardSeen = 2,
  kMaxValue = kAnswerCardSeen,
};

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
  ZERO_STATE_DRIVE,
  // A result from the Assistant provider.
  ASSISTANT,
  // An OsSettingsResult.
  OS_SETTINGS,
  // A Plugin VM App Result.
  PLUGIN_VM_APP,
  // LaCrOS binary.
  LACROS,
  // A Remote App Result.
  REMOTE_APP,
  // A Borealis App Result.
  BOREALIS_APP,
  // A Help App (aka Explore) Result. For default or help results. There is a
  // different search result type for Updates.
  HELP_APP_DEFAULT,
  // A result from omnibox for query suggestion.
  OMNIBOX_SEARCH_SUGGEST_ENTITY,
  // A result from omnibox for suggested navigation.
  OMNIBOX_NAVSUGGEST,
  // An answer result from Omnibox.
  OMNIBOX_ANSWER,
  // A calculator result from Omnibox.
  OMNIBOX_CALCULATOR,
  // A local file search result.
  FILE_SEARCH,
  // A Drive file search result.
  DRIVE_SEARCH,
  // A Help App result about the "What's new" (Updates) page.
  HELP_APP_UPDATES,
  // A Help App result about the "Discover" page. (Deprecated).
  HELP_APP_DISCOVER_DEPRECATED,
  // A keyboard shortcut result from the Keyboard Shortcut provider.
  KEYBOARD_SHORTCUT,
  // A keyboard shortcut result from the Keyboard Shortcut provider.
  OPEN_TAB,
  // Null result type that indicates that user did not interact with any results
  // in some metrics.
  NO_RESULT,
  // A game search result.
  GAME_SEARCH,
  // A search result for OS personalization options.
  PERSONALIZATION,
  // A Bruschetta App Result.
  BRUSCHETTA_APP,
  // A System Info Answer Card Result.
  SYSTEM_INFO,
  // A local image search result.
  IMAGE_SEARCH,
  // A zero-state result representing a admin template.
  DESKS_ADMIN_TEMPLATE,
  // New app shortcuts.
  APP_SHORTCUTS_V2,
  // Boundary is always last.
  SEARCH_RESULT_TYPE_BOUNDARY
};

// Sub-types defined for zero state file/drive suggestions that indicate
// the reason the file result was suggested.
// Used for metrics - assigned values should not change.
enum class ContinueFileSuggestionType {
  // For zero state drive suggestions - file suggested because the user
  // viewed it recently.
  kViewedDrive = 0,
  // For zero state drive suggestions - file suggested because it was
  // recently modified (usually by another user).
  kModifiedDrive = 1,
  // For zero state drive suggestions - file suggested because the user
  // modified it recently.
  kModifiedByCurrentUserDrive = 2,
  // For zero state drive suggestions - file suggested because it was recently
  // shared with the user.
  kSharedWithUserDrive = 3,
  // For zero state local file suggestions - file suggested because the user
  // viewed it recently.
  kViewedFile = 4,
  // For zero state local file suggestions - file suggested because the user
  // modified it recently.
  kModifiedByCurrentUserFile = 5,
  kMaxValue = kModifiedByCurrentUserFile,
};

ASH_PUBLIC_EXPORT std::string SearchSessionConclusionToString(
    SearchSessionConclusion conclusion);

// Returns true if the `show_source` is one that a user directly triggers.
ASH_PUBLIC_EXPORT bool IsAppListShowSourceUserTriggered(
    AppListShowSource show_source);

ASH_PUBLIC_EXPORT void RecordSearchResultOpenTypeHistogram(
    AppListLaunchedFrom launch_location,
    SearchResultType type,
    bool is_tablet_mode);

ASH_PUBLIC_EXPORT void RecordDefaultSearchResultOpenTypeHistogram(
    SearchResultType type);

ASH_PUBLIC_EXPORT void RecordZeroStateSuggestionOpenTypeHistogram(
    SearchResultType type);

ASH_PUBLIC_EXPORT void RecordLauncherIssuedSearchQueryLength(int query_length);

ASH_PUBLIC_EXPORT void RecordLauncherClickedSearchQueryLength(int query_length);

ASH_PUBLIC_EXPORT void RecordSuccessfulAppLaunchUsingSearch(
    AppListLaunchedFrom launched_from,
    int query_length);

ASH_PUBLIC_EXPORT void ReportPrefOrderClearAction(
    AppListOrderUpdateEvent action,
    bool in_tablet);

ASH_PUBLIC_EXPORT void RecordFirstSearchResult(SearchResultType type,
                                               bool in_tablet);

ASH_PUBLIC_EXPORT void ReportPrefSortOrderOnSessionStart(
    ash::AppListSortOrder permanent_order,
    bool in_tablet);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_METRICS_H_
