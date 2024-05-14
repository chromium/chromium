// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/types_util.h"

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"

namespace app_list {

std::string ResultTypeToString(const ash::AppListSearchResultType result_type) {
  switch (result_type) {
    case ash::AppListSearchResultType::kUnknown:
      return "Unknown";
    case ash::AppListSearchResultType::kInstalledApp:
      return "Installed app";
    case ash::AppListSearchResultType::kZeroStateApp:
      return "Zero state app";
    case ash::AppListSearchResultType::kPlayStoreApp:
      return "Play store app";
    case ash::AppListSearchResultType::kInstantApp:
      return "Instant app";
    case ash::AppListSearchResultType::kInternalApp:
      return "Internal app";
    case ash::AppListSearchResultType::kOmnibox:
      return "Omnibox";
    case ash::AppListSearchResultType::kLauncher:
      return "Launcher (files)";
    case ash::AppListSearchResultType::kAnswerCard:
      return "Answer card";
    case ash::AppListSearchResultType::kPlayStoreReinstallApp:
      return "Play store reinstall app";
    case ash::AppListSearchResultType::kArcAppShortcut:
      return "Arc app shortcut";
    case ash::AppListSearchResultType::kZeroStateFile:
      return "Zero state file";
    case ash::AppListSearchResultType::kZeroStateDrive:
      return "Zero state drive";
    case ash::AppListSearchResultType::kOsSettings:
      return "OS settings";
    case ash::AppListSearchResultType::kInternalPrivacyInfo:
      return "Internal privacy info";
    case ash::AppListSearchResultType::kAssistantText:
      return "Assistant text";
    case ash::AppListSearchResultType::kHelpApp:
      return "Help app";
    case ash::AppListSearchResultType::kZeroStateHelpApp:
      return "Help app - zero state";
    case ash::AppListSearchResultType::kFileSearch:
      return "File search";
    case ash::AppListSearchResultType::kDriveSearch:
      return "Drive search";
    case ash::AppListSearchResultType::kKeyboardShortcut:
      return "Keyboard shortcut";
    case ash::AppListSearchResultType::kOpenTab:
      return "Open tab";
    case ash::AppListSearchResultType::kGames:
      return "Games";
    case ash::AppListSearchResultType::kPersonalization:
      return "Personalization";
    case ash::AppListSearchResultType::kImageSearch:
      return "Image search";
    case ash::AppListSearchResultType::kSystemInfo:
      return "System info";
    case ash::AppListSearchResultType::kDesksAdminTemplate:
      return "Desks Admin template";
    case ash::AppListSearchResultType::kAppShortcutV2:
      return "App shortcut V2";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string MetricsTypeToString(const ash::SearchResultType metrics_type) {
  switch (metrics_type) {
    case ash::SearchResultType::OMNIBOX_SEARCH_RESULT_DEPRECATED:
      return "OMNIBOX_SEARCH_RESULT_DEPRECATED";
    case ash::SearchResultType::APP_SEARCH_RESULT_DEPRECATED:
      return "APP_SEARCH_RESULT_DEPRECATED";
    case ash::SearchResultType::WEBSTORE_SEARCH_RESULT_DEPRECATED:
      return "WEBSTORE_SEARCH_RESULT_DEPRECATED";
    case ash::SearchResultType::SEARCH_WEBSTORE_SEARCH_RESULT_DEPRECATED:
      return "SEARCH_WEBSTORE_SEARCH_RESULT_DEPRECATED";
    case ash::SearchResultType::SEARCH_PEOPLE_SEARCH_RESULT_DEPRECATED:
      return "SEARCH_PEOPLE_SEARCH_RESULT_DEPRECATED";
    case ash::SearchResultType::SUGGESTIONS_SEARCH_RESULT_DEPRECATED:
      return "SUGGESTIONS_SEARCH_RESULT_DEPRECATED";
    case ash::SearchResultType::LAUNCHER_SEARCH_PROVIDER_RESULT:
      return "LAUNCHER_SEARCH_PROVIDER_RESULT";
    case ash::SearchResultType::PLAY_STORE_UNINSTALLED_APP:
      return "PLAY_STORE_UNINSTALLED_APP";
    case ash::SearchResultType::PLAY_STORE_INSTANT_APP:
      return "PLAY_STORE_INSTANT_APP";
    case ash::SearchResultType::ANSWER_CARD:
      return "ANSWER_CARD";
    case ash::SearchResultType::PLAY_STORE_APP_SHORTCUT:
      return "PLAY_STORE_APP_SHORTCUT";
    case ash::SearchResultType::OMNIBOX_URL_WHAT_YOU_TYPED:
      return "OMNIBOX_URL_WHAT_YOU_TYPED";
    case ash::SearchResultType::OMNIBOX_BOOKMARK:
      return "OMNIBOX_BOOKMARK";
    case ash::SearchResultType::OMNIBOX_RECENTLY_VISITED_WEBSITE:
      return "OMNIBOX_RECENTLY_VISITED_WEBSITE";
    case ash::SearchResultType::OMNIBOX_RECENT_DOC_IN_DRIVE:
      return "OMNIBOX_RECENT_DOC_IN_DRIVE";
    case ash::SearchResultType::OMNIBOX_WEB_QUERY:
      return "OMNIBOX_WEB_QUERY";
    case ash::SearchResultType::OMNIBOX_HISTORY_DEPRECATED:
      return "OMNIBOX_HISTORY_DEPRECATED";
    case ash::SearchResultType::PLAY_STORE_APP:
      return "PLAY_STORE_APP";
    case ash::SearchResultType::PLAY_STORE_REINSTALL_APP:
      return "PLAY_STORE_REINSTALL_APP";
    case ash::SearchResultType::INTERNAL_APP:
      return "INTERNAL_APP";
    case ash::SearchResultType::EXTENSION_APP:
      return "EXTENSION_APP";
    case ash::SearchResultType::CROSTINI_APP:
      return "CROSTINI_APP";
    case ash::SearchResultType::SETTINGS_SHORTCUT:
      return "SETTINGS_SHORTCUT";
    case ash::SearchResultType::APP_DATA_RESULT_PERSON:
      return "APP_DATA_RESULT_PERSON";
    case ash::SearchResultType::APP_DATA_RESULT_NOTE_DOCUMENT:
      return "APP_DATA_RESULT_NOTE_DOCUMENT";
    case ash::SearchResultType::ASSISTANT_OMNIBOX_RESULT:
      return "ASSISTANT_OMNIBOX_RESULT";
    case ash::SearchResultType::OMNIBOX_SEARCH_HISTORY:
      return "OMNIBOX_SEARCH_HISTORY";
    case ash::SearchResultType::OMNIBOX_SEARCH_SUGGEST:
      return "OMNIBOX_SEARCH_SUGGEST";
    case ash::SearchResultType::OMNIBOX_SUGGEST_PERSONALIZED:
      return "OMNIBOX_SUGGEST_PERSONALIZED";
    case ash::SearchResultType::ZERO_STATE_FILE:
      return "ZERO_STATE_FILE";
    case ash::SearchResultType::ZERO_STATE_DRIVE:
      return "ZERO_STATE_DRIVE";
    case ash::SearchResultType::ASSISTANT:
      return "ASSISTANT";
    case ash::SearchResultType::OS_SETTINGS:
      return "OS_SETTINGS";
    case ash::SearchResultType::PLUGIN_VM_APP:
      return "PLUGIN_VM_APP";
    case ash::SearchResultType::LACROS:
      return "LACROS";
    case ash::SearchResultType::REMOTE_APP:
      return "REMOTE_APP";
    case ash::SearchResultType::BOREALIS_APP:
      return "BOREALIS_APP";
    case ash::SearchResultType::BRUSCHETTA_APP:
      return "BRUSCHETTA_APP";
    case ash::SearchResultType::HELP_APP_DEFAULT:
      return "HELP_APP_DEFAULT";
    case ash::SearchResultType::OMNIBOX_SEARCH_SUGGEST_ENTITY:
      return "OMNIBOX_SEARCH_SUGGEST_ENTITY";
    case ash::SearchResultType::OMNIBOX_NAVSUGGEST:
      return "OMNIBOX_NAVSUGGEST";
    case ash::SearchResultType::OMNIBOX_ANSWER:
      return "OMNIBOX_ANSWER";
    case ash::SearchResultType::OMNIBOX_CALCULATOR:
      return "OMNIBOX_CALCULATOR";
    case ash::SearchResultType::FILE_SEARCH:
      return "FILE_SEARCH";
    case ash::SearchResultType::DRIVE_SEARCH:
      return "DRIVE_SEARCH";
    case ash::SearchResultType::HELP_APP_UPDATES:
      return "HELP_APP_UPDATES";
    case ash::SearchResultType::HELP_APP_DISCOVER_DEPRECATED:
      return "HELP_APP_DISCOVER_DEPRECATED";
    case ash::SearchResultType::KEYBOARD_SHORTCUT:
      return "KEYBOARD_SHORTCUT";
    case ash::SearchResultType::OPEN_TAB:
      return "OPEN_TAB";
    case ash::SearchResultType::NO_RESULT:
      return "NO_RESULT";
    case ash::SearchResultType::GAME_SEARCH:
      return "GAME_SEARCH";
    case ash::SearchResultType::PERSONALIZATION:
      return "PERSONALIZATION";
    case ash::SearchResultType::SEARCH_RESULT_TYPE_BOUNDARY:
      return "SEARCH_RESULT_TYPE_BOUNDARY";
    case ash::SearchResultType::SYSTEM_INFO:
      return "SYSTEM_INFO";
    case ash::SearchResultType::IMAGE_SEARCH:
      return "IMAGE_SEARCH";
    case ash::SearchResultType::DESKS_ADMIN_TEMPLATE:
      return "DESKS_ADMIN_TEMPLATE";
    case ash::SearchResultType::APP_SHORTCUTS_V2:
      return "APP_SHORTCUTS_V2";
  }
  NOTREACHED_IN_MIGRATION();
}

std::string DisplayTypeToString(
    const ash::SearchResultDisplayType display_type) {
  switch (display_type) {
    case ash::SearchResultDisplayType::kNone:
    case ash::SearchResultDisplayType::kLast:
      return "None";
    case ash::SearchResultDisplayType::kList:
      return "List";
    case ash::SearchResultDisplayType::kAnswerCard:
      return "AnswerCard";
    case ash::SearchResultDisplayType::kContinue:
      return "Continue";
    case ash::SearchResultDisplayType::kRecentApps:
      return "RecentApps";
    case ash::SearchResultDisplayType::kImage:
      return "Image";
  }
  NOTREACHED_IN_MIGRATION();
}

ash::AppListSearchControlCategory MapSearchCategoryToControlCategory(
    SearchCategory search_category) {
  switch (search_category) {
    case SearchCategory::kApps:
      return ash::AppListSearchControlCategory::kApps;
    case SearchCategory::kAppShortcuts:
      return ash::AppListSearchControlCategory::kAppShortcuts;
    case SearchCategory::kFiles:
      return ash::AppListSearchControlCategory::kFiles;
    case SearchCategory::kGames:
      return ash::AppListSearchControlCategory::kGames;
    case SearchCategory::kHelp:
      return ash::AppListSearchControlCategory::kHelp;
    case SearchCategory::kImages:
      return ash::AppListSearchControlCategory::kImages;
    case SearchCategory::kPlayStore:
      return ash::AppListSearchControlCategory::kPlayStore;
    case SearchCategory::kWeb:
      return ash::AppListSearchControlCategory::kWeb;
    case SearchCategory::kSettings:
    case SearchCategory::kOmnibox:
    case SearchCategory::kTest:
    case SearchCategory::kDesksAdmin:
    case SearchCategory::kAssistant:
    case SearchCategory::kSystemInfoCard:
      return ash::AppListSearchControlCategory::kCannotToggle;
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace app_list
