// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_metrics.h"

#include <algorithm>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"

namespace ash {

namespace {

int CalculateAnimationSmoothness(int actual_frames,
                                 base::TimeDelta ideal_duration,
                                 float refresh_rate) {
  int smoothness = 100;
  const int ideal_frames = refresh_rate * ideal_duration.InSecondsF();
  if (ideal_frames > actual_frames)
    smoothness = 100 * actual_frames / ideal_frames;
  return smoothness;
}

// These constants affect logging, and  should not be changed without
// deprecating the following UMA histograms:
//  - Apps.AppListTileClickIndexAndQueryLength
//  - Apps.AppListResultClickIndexAndQueryLength
constexpr int kMaxLoggedQueryLength = 10;
constexpr int kMaxLoggedSuggestionIndex = 6;
constexpr int kMaxLoggedHistogramValue =
    (kMaxLoggedSuggestionIndex + 1) * kMaxLoggedQueryLength +
    kMaxLoggedSuggestionIndex;

}  // namespace

// The UMA histogram that logs smoothness of folder show/hide animation.
constexpr char kFolderShowHideAnimationSmoothness[] =
    "Apps.AppListFolder.ShowHide.AnimationSmoothness";

// The UMA histogram that logs smoothness of pagination animation.
constexpr char kPaginationTransitionAnimationSmoothness[] =
    "Apps.PaginationTransition.AnimationSmoothness";
constexpr char kPaginationTransitionAnimationSmoothnessInTablet[] =
    "Apps.PaginationTransition.AnimationSmoothness.TabletMode";
constexpr char kPaginationTransitionAnimationSmoothnessInClamshell[] =
    "Apps.PaginationTransition.AnimationSmoothness.ClamshellMode";

// The UMA histogram that logs which state search results are opened from.
constexpr char kAppListSearchResultOpenSourceHistogram[] =
    "Apps.AppListSearchResultOpenedSource";

// The UMA hisotogram that logs the action user performs on zero state
// search result.
constexpr char kAppListZeroStateSearchResultUserActionHistogram[] =
    "Apps.AppList.ZeroStateSearchResultUserActionType";

// The UMA histogram that logs user's decision(remove or cancel) for zero state
// search result removal confirmation.
constexpr char kAppListZeroStateSearchResultRemovalHistogram[] =
    "Apps.AppList.ZeroStateSearchResultRemovalDecision";

// The UMA histogram that logs the length of the query when user abandons
// results of a queried search or recommendations of zero state(zero length
// query) in launcher UI.
constexpr char kSearchAbandonQueryLengthHistogram[] =
    "Apps.AppListSearchAbandonQueryLength";

// The base UMA histogram that logs app launches within the AppList and shelf.
constexpr char kAppListAppLaunched[] = "Apps.AppListAppLaunchedV2";

// The UMA histograms that log app launches within the AppList and shelf. The
// app launches are divided by histogram for each of the the different AppList
// states.
constexpr char kAppListAppLaunchedClosed[] = "Apps.AppListAppLaunchedV2.Closed";
constexpr char kAppListAppLaunchedPeeking[] =
    "Apps.AppListAppLaunchedV2.Peeking";
constexpr char kAppListAppLaunchedHalf[] = "Apps.AppListAppLaunchedV2.Half";
constexpr char kAppListAppLaunchedFullscreenAllApps[] =
    "Apps.AppListAppLaunchedV2.FullscreenAllApps";
constexpr char kAppListAppLaunchedFullscreenSearch[] =
    "Apps.AppListAppLaunchedV2.FullscreenSearch";
constexpr char kAppListAppLaunchedHomecherClosed[] =
    "Apps.AppListAppLaunchedV2.HomecherClosed";
constexpr char kAppListAppLaunchedHomecherAllApps[] =
    "Apps.AppListAppLaunchedV2.HomecherAllApps";
constexpr char kAppListAppLaunchedHomecherSearch[] =
    "Apps.AppListAppLaunchedV2.HomecherSearch";

// The different sources from which a search result is displayed. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum class ApplistSearchResultOpenedSource {
  kHalfClamshell = 0,
  kFullscreenClamshell = 1,
  kFullscreenTablet = 2,
  kMaxApplistSearchResultOpenedSource = 3,
};

void RecordFolderShowHideAnimationSmoothness(int actual_frames,
                                             base::TimeDelta ideal_duration,
                                             float refresh_rate) {
  const int smoothness =
      CalculateAnimationSmoothness(actual_frames, ideal_duration, refresh_rate);
  UMA_HISTOGRAM_PERCENTAGE(kFolderShowHideAnimationSmoothness, smoothness);
}

void RecordPaginationAnimationSmoothness(int actual_frames,
                                         base::TimeDelta ideal_duration,
                                         float refresh_rate,
                                         bool is_tablet_mode) {
  const int smoothness =
      CalculateAnimationSmoothness(actual_frames, ideal_duration, refresh_rate);
  UMA_HISTOGRAM_PERCENTAGE(kPaginationTransitionAnimationSmoothness,
                           smoothness);
  if (is_tablet_mode) {
    UMA_HISTOGRAM_PERCENTAGE(kPaginationTransitionAnimationSmoothnessInTablet,
                             smoothness);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        kPaginationTransitionAnimationSmoothnessInClamshell, smoothness);
  }
}

void AppListRecordPageSwitcherSourceByEventType(ui::EventType type,
                                                bool is_tablet_mode) {
  AppListPageSwitcherSource source;

  switch (type) {
    case ui::ET_MOUSEWHEEL:
      source = kMouseWheelScroll;
      break;
    case ui::ET_SCROLL:
      source = kMousePadScroll;
      break;
    case ui::ET_GESTURE_SCROLL_END:
      source = kSwipeAppGrid;
      break;
    case ui::ET_SCROLL_FLING_START:
      source = kFlingAppGrid;
      break;
    case ui::ET_MOUSE_RELEASED:
      source = kMouseDrag;
      break;
    default:
      NOTREACHED();
      return;
  }
  RecordPageSwitcherSource(source, is_tablet_mode);
}

void RecordPageSwitcherSource(AppListPageSwitcherSource source,
                              bool is_tablet_mode) {
  UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogram, source,
                            kMaxAppListPageSwitcherSource);
  if (is_tablet_mode) {
    UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogramInTablet,
                              source, kMaxAppListPageSwitcherSource);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogramInClamshell,
                              source, kMaxAppListPageSwitcherSource);
  }
}

APP_LIST_EXPORT void RecordSearchResultOpenSource(
    const SearchResult* result,
    const AppListModel* model,
    const SearchModel* search_model) {
  // Record the search metric if the SearchResult is not a suggested app.
  if (result->display_type() == ash::SearchResultDisplayType::kRecommendation)
    return;

  ApplistSearchResultOpenedSource source;
  ash::AppListViewState state = model->state_fullscreen();
  if (search_model->tablet_mode()) {
    source = ApplistSearchResultOpenedSource::kFullscreenTablet;
  } else {
    source = state == ash::AppListViewState::kHalf
                 ? ApplistSearchResultOpenedSource::kHalfClamshell
                 : ApplistSearchResultOpenedSource::kFullscreenClamshell;
  }
  UMA_HISTOGRAM_ENUMERATION(
      kAppListSearchResultOpenSourceHistogram, source,
      ApplistSearchResultOpenedSource::kMaxApplistSearchResultOpenedSource);
}

void RecordSearchLaunchIndexAndQueryLength(
    SearchResultLaunchLocation launch_location,
    int query_length,
    int suggestion_index) {
  if (suggestion_index < 0) {
    LOG(ERROR) << "Received invalid suggestion index.";
    return;
  }

  query_length = std::min(query_length, kMaxLoggedQueryLength);
  suggestion_index = std::min(suggestion_index, kMaxLoggedSuggestionIndex);
  const int logged_value =
      (kMaxLoggedSuggestionIndex + 1) * query_length + suggestion_index;

  if (launch_location == SearchResultLaunchLocation::kResultList) {
    UMA_HISTOGRAM_EXACT_LINEAR(kAppListResultLaunchIndexAndQueryLength,
                               logged_value, kMaxLoggedHistogramValue);
  } else if (launch_location == SearchResultLaunchLocation::kTileList) {
    UMA_HISTOGRAM_EXACT_LINEAR(kAppListTileLaunchIndexAndQueryLength,
                               logged_value, kMaxLoggedHistogramValue);
  }
}

void RecordSearchAbandonWithQueryLengthHistogram(int query_length) {
  UMA_HISTOGRAM_EXACT_LINEAR(kSearchAbandonQueryLengthHistogram,
                             std::min(query_length, kMaxLoggedQueryLength),
                             kMaxLoggedQueryLength);
}

void RecordZeroStateSearchResultUserActionHistogram(
    ZeroStateSearchResultUserActionType action) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSearchResultUserActionHistogram,
                            action);
}

void RecordZeroStateSearchResultRemovalHistogram(
    ZeroStateSearchResutRemovalConfirmation removal_decision) {
  UMA_HISTOGRAM_ENUMERATION(kAppListZeroStateSearchResultRemovalHistogram,
                            removal_decision);
}

void RecordAppListAppLaunched(ash::AppListLaunchedFrom launched_from,
                              ash::AppListViewState app_list_state,
                              bool is_tablet_mode,
                              bool home_launcher_shown) {
  UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunched, launched_from);
  switch (app_list_state) {
    case ash::AppListViewState::kClosed:
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedClosed, launched_from);
      break;
    case ash::AppListViewState::kPeeking:
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedPeeking, launched_from);
      break;
    case ash::AppListViewState::kHalf:
      UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHalf, launched_from);
      break;
    case ash::AppListViewState::kFullscreenAllApps:
      if (is_tablet_mode) {
        if (home_launcher_shown) {
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherAllApps,
                                    launched_from);
        } else {
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherClosed,
                                    launched_from);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedFullscreenAllApps,
                                  launched_from);
      }
      break;
    case ash::AppListViewState::kFullscreenSearch:
      if (is_tablet_mode) {
        if (home_launcher_shown) {
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherSearch,
                                    launched_from);
        } else {
          // (http://crbug.com/947729) Search box still expanded when opening
          // launcher in tablet mode
          UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedHomecherClosed,
                                    launched_from);
        }
      } else {
        UMA_HISTOGRAM_ENUMERATION(kAppListAppLaunchedFullscreenSearch,
                                  launched_from);
      }
      break;
  }
}

bool IsCommandIdAnAppLaunch(int command_id_number) {
  ash::CommandId command_id = static_cast<ash::CommandId>(command_id_number);

  // Consider all platform app menu options as launches.
  if (command_id >= ash::CommandId::EXTENSIONS_CONTEXT_CUSTOM_FIRST &&
      command_id < ash::CommandId::EXTENSIONS_CONTEXT_CUSTOM_LAST) {
    return true;
  }

  // Consider all arc app shortcut options as launches.
  if (command_id >= ash::CommandId::LAUNCH_APP_SHORTCUT_FIRST &&
      command_id < ash::CommandId::LAUNCH_APP_SHORTCUT_LAST) {
    return true;
  }

  switch (command_id) {
    // Used by LauncherContextMenu (shelf).
    case ash::CommandId::MENU_OPEN_NEW:
    case ash::CommandId::MENU_NEW_WINDOW:
    case ash::CommandId::MENU_NEW_INCOGNITO_WINDOW:
    // Used by AppContextMenu.
    case ash::CommandId::LAUNCH_NEW:
    case ash::CommandId::SHOW_APP_INFO:
    case ash::CommandId::OPTIONS:
    case ash::CommandId::APP_CONTEXT_MENU_NEW_WINDOW:
    case ash::CommandId::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW:
    // Used by both AppContextMenu and LauncherContextMenu for app shortcuts.
    case ash::CommandId::LAUNCH_APP_SHORTCUT_FIRST:
    case ash::CommandId::LAUNCH_APP_SHORTCUT_LAST:
      return true;

    // Used by LauncherContextMenu (shelf).
    case ash::CommandId::MENU_CLOSE:
    case ash::CommandId::MENU_PIN:
    case ash::CommandId::LAUNCH_TYPE_PINNED_TAB:
    case ash::CommandId::LAUNCH_TYPE_REGULAR_TAB:
    case ash::CommandId::LAUNCH_TYPE_FULLSCREEN:
    case ash::CommandId::LAUNCH_TYPE_WINDOW:
    // Used by AppMenuModelAdapter
    case ash::CommandId::NOTIFICATION_CONTAINER:
    // Used by CrostiniShelfContextMenu.
    case ash::CommandId::CROSTINI_USE_LOW_DENSITY:
    case ash::CommandId::CROSTINI_USE_HIGH_DENSITY:
    // Used by AppContextMenu.
    case ash::CommandId::TOGGLE_PIN:
    case ash::CommandId::UNINSTALL:
    case ash::CommandId::REMOVE_FROM_FOLDER:
    case ash::CommandId::INSTALL:
    case ash::CommandId::USE_LAUNCH_TYPE_PINNED:
    case ash::CommandId::USE_LAUNCH_TYPE_REGULAR:
    case ash::CommandId::USE_LAUNCH_TYPE_FULLSCREEN:
    case ash::CommandId::USE_LAUNCH_TYPE_WINDOW:
    case ash::CommandId::USE_LAUNCH_TYPE_COMMAND_END:
    case ash::CommandId::STOP_APP:
    case ash::CommandId::EXTENSIONS_CONTEXT_CUSTOM_FIRST:
    case ash::CommandId::EXTENSIONS_CONTEXT_CUSTOM_LAST:
    case ash::CommandId::COMMAND_ID_COUNT:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace ash
