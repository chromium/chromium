// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_METRICS_H_
#define ASH_APP_LIST_APP_LIST_METRICS_H_

#include "ash/app_list/app_list_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ui/events/event.h"

namespace ash {

class AppListModel;
class SearchModel;
class SearchResult;

// The UMA histogram that logs the input latency from input event to the
// representation time of the shown launcher UI.
constexpr char kAppListShowInputLatencyHistogram[] =
    "Apps.AppListShow.InputLatency";

// The UMA histogram that logs the input latency from input event to the
// representation time of the dismissed launcher UI.
constexpr char kAppListHideInputLatencyHistogram[] =
    "Apps.AppListHide.InputLatency";

// The UMA histogram that logs different ways to move an app in app list's apps
// grid.
constexpr char kAppListAppMovingType[] = "Apps.AppListAppMovingType";

// The UMA histogram that logs the creation time of the AppListView.
constexpr char kAppListCreationTimeHistogram[] = "Apps.AppListCreationTime";

// The UMA histogram that logs usage of state transitions in the new
// app list UI.
constexpr char kAppListStateTransitionSourceHistogram[] =
    "Apps.AppListStateTransitionSource";

// The UMA histogram that logs the source of root app grid page switcher usage
// in the app list.
constexpr char kAppListPageSwitcherSourceHistogram[] =
    "Apps.AppListPageSwitcherSource";

// The UMA histogram that logs the source of root app grid page switcher usage
// in the app list in tablet mode.
constexpr char kAppListPageSwitcherSourceHistogramInTablet[] =
    "Apps.AppListPageSwitcherSource.TabletMode";

// The UMA histogram that logs the source of root app grid page switcher usage
// in the app list in clamshell mode.
constexpr char kAppListPageSwitcherSourceHistogramInClamshell[] =
    "Apps.AppListPageSwitcherSource.ClamshellMode";

// The UMA histogram that logs usage of the original and redesigned folders.
constexpr char kAppListFolderOpenedHistogram[] = "Apps.AppListFolderOpened";

// The UMA histogram that logs how the app list transitions from peeking to
// fullscreen.
constexpr char kAppListPeekingToFullscreenHistogram[] =
    "Apps.AppListPeekingToFullscreenSource";

// The UMA histogram that logs how the app list is shown.
constexpr char kAppListToggleMethodHistogram[] = "Apps.AppListShowSource";

// The UMA histogram that logs the index launched item in the results list and
// the query length.
constexpr char kAppListResultLaunchIndexAndQueryLength[] =
    "Apps.AppListResultLaunchIndexAndQueryLength";

// The UMA histogram that logs the index launched item in the app tile list and
// the query length.
constexpr char kAppListTileLaunchIndexAndQueryLength[] =
    "Apps.AppListTileLaunchIndexAndQueryLength";

// The UMA histogram that logs the presence or absence of Drive QuickAccess
// search results in the zero-state results list. Differentiates between results
// existing in the model's results list, but not being displayed in the view.
constexpr char kDriveQuickAccessResultPresence[] =
    "Apps.AppListDriveQuickAccessProvider.ResultPresence";

// The UMA histogram that logs which page gets opened by the user.
constexpr char kPageOpenedHistogram[] = "Apps.AppListPageOpened";

// The UMA histogram that logs how many apps users have in folders.
constexpr char kNumberOfAppsInFoldersHistogram[] =
    "Apps.AppsInFolders.FullscreenAppListEnabled";

// The UMA histogram that logs how many folders users have.
constexpr char kNumberOfFoldersHistogram[] = "Apps.NumberOfFolders";

// The UMA histogram that logs how many pages users have in top level apps grid.
constexpr char kNumberOfPagesHistogram[] = "Apps.NumberOfPages";

// The UMA histogram that logs how many pages with empty slots users have in top
// level apps grid.
constexpr char kNumberOfPagesNotFullHistogram[] = "Apps.NumberOfPagesNotFull";

// The UMA histogram that logs the type of search result opened.
constexpr char kSearchResultOpenDisplayTypeHistogram[] =
    "Apps.AppListSearchResultOpenDisplayType";

// The UMA histogram that logs how long the search query was when a result was
// opened.
constexpr char kSearchQueryLength[] = "Apps.AppListSearchQueryLength";

// The UMA histogram that logs how long the search query was when a result was
// opened in clamshell mode.
constexpr char kSearchQueryLengthInClamshell[] =
    "Apps.AppListSearchQueryLength.ClamshellMode";

// The UMA histogram that logs how long the search query was when a result was
// opened in tablet mode.
constexpr char kSearchQueryLengthInTablet[] =
    "Apps.AppListSearchQueryLength.TabletMode";

// The UMA histogram that logs the Manhattan distance from the origin of the
// search results to the selected result.
constexpr char kSearchResultDistanceFromOrigin[] =
    "Apps.AppListSearchResultDistanceFromOrigin";

// The different ways to create a new page in the apps grid. These values are
// written to logs. New enum values can be added, but existing enums must never
// be renumbered or deleted and reused.
enum class AppListPageCreationType {
  kDraggingApp = 0,
  kMovingAppWithKeyboard = 1,
  kSyncOrInstall = 2,
  kMaxValue = kSyncOrInstall,
};

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListZeroStateSearchResultUserActionType enum listing in
// tools/metrics/histograms/enums.xml.
enum class ZeroStateSearchResultUserActionType {
  kRemoveResult = 0,
  kAppendResult = 1,
  kMaxValue = kAppendResult,
};

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListZeroStateResultRemovalConfirmation enum listing in
// tools/metrics/histograms/enums.xml.
enum class ZeroStateSearchResutRemovalConfirmation {
  kRemovalConfirmed = 0,
  kRemovalCanceled = 1,
  kMaxValue = kRemovalCanceled,
};

// The different ways that the app list can transition from PEEKING to
// FULLSCREEN_ALL_APPS. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListPeekingToFullscreenSource {
  kSwipe = 0,
  kExpandArrow = 1,
  kMousepadScroll = 2,
  kMousewheelScroll = 3,
  kMaxPeekingToFullscreen = 4,
};

// The different ways the app list can be shown. These values are written to
// logs.  New enum values can be added, but existing enums must never be
// renumbered or deleted and reused.
enum AppListShowSource {
  kSearchKey = 0,
  kShelfButton = 1,
  kSwipeFromShelf = 2,
  kTabletMode = 3,
  kSearchKeyFullscreen = 4,
  kShelfButtonFullscreen = 5,
  kAssistantEntryPoint = 6,
  kScrollFromShelf = 7,
  kMaxValue = kScrollFromShelf,
};

// The two versions of folders. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListFolderOpened {
  kOldFolders = 0,
  kFullscreenAppListFolders = 1,
  kMaxFolderOpened = 2,
};

// The valid AppListState transitions. These values are written to logs.  New
// enum values can be added, but existing enums must never be renumbered or
// deleted and reused. If adding a state transition, add it to the switch
// statement in AppListView::GetAppListStateTransitionSource.
enum AppListStateTransitionSource {
  kFullscreenAllAppsToClosed = 0,
  kFullscreenAllAppsToFullscreenSearch = 1,
  kFullscreenAllAppsToPeeking = 2,
  kFullscreenSearchToClosed = 3,
  kFullscreenSearchToFullscreenAllApps = 4,
  kHalfToClosed = 5,
  KHalfToFullscreenSearch = 6,
  kHalfToPeeking = 7,
  kPeekingToClosed = 8,
  kPeekingToFullscreenAllApps = 9,
  kPeekingToHalf = 10,
  kMaxAppListStateTransition = 11,
};

// The different ways to change pages in the app list's app grid. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum AppListPageSwitcherSource {
  kTouchPageIndicator = 0,
  kClickPageIndicator = 1,
  kSwipeAppGrid = 2,
  kFlingAppGrid = 3,
  kMouseWheelScroll = 4,
  kMousePadScroll = 5,
  kDragAppToBorder = 6,
  kMoveAppWithKeyboard = 7,
  kMouseDrag = 8,
  kMaxAppListPageSwitcherSource = 9,
};

// The different ways to move an app in app list's apps grid. These values are
// written to logs. New enum values can be added, but existing enums must never
// be renumbered or deleted and reused.
enum AppListAppMovingType {
  kMoveByDragIntoFolder = 0,
  kMoveByDragOutOfFolder = 1,
  kMoveIntoAnotherFolder = 2,
  kReorderByDragInFolder = 3,
  kReorderByDragInTopLevel = 4,
  kReorderByKeyboardInFolder = 5,
  kReorderByKeyboardInTopLevel = 6,
  kMoveByKeyboardIntoFolder = 7,
  kMoveByKeyboardOutOfFolder = 8,
  kMaxAppListAppMovingType = 9,
};

// The presence of Drive QuickAccess search results when updating the zero-state
// results list. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class DriveQuickAccessResultPresence {
  kPresentAndShown = 0,
  kPresentAndNotShown = 1,
  kAbsent = 2,
  kMaxValue = kAbsent
};

// Different places a search result can be launched from. These values do not
// persist to logs, so can be changed as-needed. However, changes should be
// reflected in RecordSearchLaunchIndexAndQueryLength().
enum SearchResultLaunchLocation {
  kResultList = 0,
  kTileList = 1,
};

// Different ways to trigger launcher animation in tablet mode.
enum TabletModeAnimationTransition {
  // Release drag to show the launcher (launcher animates the rest of the way).
  kDragReleaseShow,

  // Release drag to hide the launcher (launcher animates the rest of the way).
  kDragReleaseHide,

  // Click the Home button in tablet mode.
  kHomeButtonShow,

  // Activate a window from shelf to hide the launcher in tablet mode.
  kHideHomeLauncherForWindow,

  // Enter the overview mode in tablet
  kEnterOverviewMode,

  // Exit the overview mode in tablet
  kExitOverviewMode,

  // Enter the kFullscreenAllApps state (usually by deactivating the search box)
  kEnterFullscreenAllApps,

  // Enter the kFullscreenSearch state (usually by activating the search box).
  kEnterFullscreenSearch,

  // Enter the overview mode in tablet, with overview fading in instead of
  // sliding (as is the case with kEnterOverviewMode).
  kFadeInOverview,

  // Exit the overview mode in tablet, with overview fading out instead of
  // sliding (as is the case with kExitOverviewMode).
  kFadeOutOverview,
};

// Parameters to call RecordAppListAppLaunched. Passed to code that does not
// directly have access to them, such ash AppListMenuModelAdapter.
struct AppLaunchedMetricParams {
  ash::AppListLaunchedFrom launched_from =
      ash::AppListLaunchedFrom::kLaunchedFromGrid;
  ash::AppListLaunchType search_launch_type =
      ash::AppListLaunchType::kSearchResult;
  ash::AppListViewState app_list_view_state = ash::AppListViewState::kClosed;
  bool is_tablet_mode = false;
  bool home_launcher_shown = false;
};

void RecordFolderShowHideAnimationSmoothness(int actual_frames,
                                             base::TimeDelta ideal_duration,
                                             float refresh_rate);

void AppListRecordPageSwitcherSourceByEventType(ui::EventType type,
                                                bool is_tablet_mode);

void RecordPageSwitcherSource(AppListPageSwitcherSource source,
                              bool is_tablet_mode);

void RecordPaginationAnimationSmoothness(int actual_frames,
                                         base::TimeDelta ideal_duration,
                                         float refresh_rate,
                                         bool is_tablet_mode);

void RecordZeroStateSearchResultUserActionHistogram(
    ZeroStateSearchResultUserActionType action);

void RecordZeroStateSearchResultRemovalHistogram(
    ZeroStateSearchResutRemovalConfirmation removal_decision);

APP_LIST_EXPORT void RecordSearchAbandonWithQueryLengthHistogram(
    int query_length);

APP_LIST_EXPORT void RecordSearchResultOpenSource(
    const SearchResult* result,
    const AppListModel* model,
    const SearchModel* search_model);

APP_LIST_EXPORT void RecordSearchLaunchIndexAndQueryLength(
    SearchResultLaunchLocation launch_location,
    int query_length,
    int suggestion_index);

APP_LIST_EXPORT void RecordAppListAppLaunched(
    ash::AppListLaunchedFrom launched_from,
    ash::AppListViewState app_list_state,
    bool is_tablet_mode,
    bool home_launcher_shown);

APP_LIST_EXPORT bool IsCommandIdAnAppLaunch(int command_id);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_METRICS_H_
