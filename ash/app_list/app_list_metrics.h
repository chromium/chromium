// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_METRICS_H_
#define ASH_APP_LIST_APP_LIST_METRICS_H_

#include <map>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/time/time.h"
#include "ui/events/event.h"

namespace ash {

// UMA histograms that record app list sort reorder animation smoothness.
// Exposed in this header because it is needed in tests.
ASH_EXPORT extern const char kClamshellReorderAnimationSmoothnessHistogram[];
ASH_EXPORT extern const char kTabletReorderAnimationSmoothnessHistogram[];

// UMA histograms that record app list sort reorder actions. Exposed in this
// header because it is needed in tests.
ASH_EXPORT extern const char kClamshellReorderActionHistogram[];
ASH_EXPORT extern const char kTabletReorderActionHistogram[];

// UMA histograms that record app list drag reorder animation smoothness.
// Exposed in this header because it is needed in tests.
ASH_EXPORT extern const char
    kClamshellDragReorderAnimationSmoothnessHistogram[];
ASH_EXPORT extern const char kTabletDragReorderAnimationSmoothnessHistogram[];

// UMA histograms that records the number of files removed per user per session
// from the launcher continue section. Exposed in this header because it is
// needed in tests.
ASH_EXPORT extern const char kContinueSectionFilesRemovedInSessionHistogram[];

// UMA histograms that records the number of times that the search category
// filter menu is opened. Exposed in this header because it is needed in tests.
extern const char kSearchCategoryFilterMenuOpened[];

// UMA histograms that records the enable state for each search category when
// filter menu is closed and the search is retriggered. Note that there must be
// a category string appended to this header to form a complete histogram name.
// Exposed in this header because it is needed in tests.
extern const char kSearchCategoriesEnableStateHeader[];

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// `AppListResultRemovalConfirmation` enum listed in
// tools/metrics/histograms/metadata/apps/enums.xml.
enum class SearchResultRemovalConfirmation {
  kRemovalConfirmed = 0,
  kRemovalCanceled = 1,
  kMaxValue = kRemovalCanceled,
};

// The two versions of folders. These values are written to logs.  New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AppListFolderOpened {
  kOldFolders = 0,
  kFullscreenAppListFolders = 1,
  kMaxFolderOpened = 2,
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
  // Click the Home button in tablet mode.
  kHomeButtonShow,

  // Activate a window from shelf to hide the launcher in tablet mode.
  kHideHomeLauncherForWindow,

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

// Different actions that complete a user workflow within the launcher UI.
// Used as bucket values in histograms that track completed user actions within
// the launcher - do not remove/renumber existing items.
enum class AppListUserAction {
  // User launched an app from the apps grid within the app list UI.
  kAppLaunchFromAppsGrid = 0,

  // User launched an app from list of recent apps within the app list UI.
  kAppLaunchFromRecentApps = 1,

  // User opened a non-app search result from the app list search results page.
  kOpenSearchResult = 2,

  // User opened an app search result from the app list search result page.
  kOpenAppSearchResult = 3,

  // User opened an item shown in continue section within the app list UI.
  kOpenContinueSectionTask = 4,

  // User opened a suggestion chip shown in the app list UI.
  DEPRECATED_kOpenSuggestionChip = 5,

  // User navigated to the bottom of the app list UI.
  kNavigatedToBottomOfAppList = 6,

  // User launched an app from list of apps collections UI.
  kAppLauncherFromAppsCollections = 7,

  kMaxValue = kAppLauncherFromAppsCollections,
};

// The possible states for a search control category. The values should match
// the AppListSearchCategoryState enum in enums.xml and should not be changed.
enum class SearchCategoryEnableState {
  // The search category is not available for users to toggle and the results
  // that belong to the category will not be shown.
  kNotAvailable = 0,

  // The search category is enabled and the results that belong to the category
  // will be shown if there is one. This is the default value for an available
  // category.
  kEnabled = 1,

  // The search category is manually disabled by users and the results that
  // belong to the category will not be shown.
  kDisabled = 2,

  kMaxValue = kDisabled,
};

enum class AppEntity {
  kDefaultApp = 0,
  kThirdPartyApp = 1,
  kMaxValue = kThirdPartyApp,
};

using CategoryEnableStateMap =
    std::map<AppListSearchControlCategory, SearchCategoryEnableState>;

// Whether and how user-entered search box text matches up with the first search
// result. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class SearchBoxTextMatch {
  // The user entered query does not match the first search result. Autocomplete
  // is not triggered.
  kNoMatch = 0,
  // The user entered query matches the prefix of the first search result.
  kPrefixMatch = 1,
  // The user entered query is a substring of the first search result.
  kSubstringMatch = 2,
  // The user's query does not match the first search result but autocomplete is
  // triggered.
  kAutocompletedWithoutMatch = 3,
  kMaxValue = kAutocompletedWithoutMatch,
};

// Parameters to call RecordAppListAppLaunched. Passed to code that does not
// directly have access to them, such ash AppListMenuModelAdapter.
struct AppLaunchedMetricParams {
  AppLaunchedMetricParams();
  AppLaunchedMetricParams(AppListLaunchedFrom launched_from,
                          AppListLaunchType launch_type);
  AppLaunchedMetricParams(const AppLaunchedMetricParams&);
  AppLaunchedMetricParams& operator=(const AppLaunchedMetricParams&);
  ~AppLaunchedMetricParams();

  AppListLaunchedFrom launched_from = AppListLaunchedFrom::kLaunchedFromGrid;
  AppListLaunchType launch_type = AppListLaunchType::kSearchResult;
  AppListViewState app_list_view_state = AppListViewState::kClosed;
  bool is_tablet_mode = false;
  bool app_list_shown = false;
  bool is_apps_collections_page = false;
  std::optional<base::TimeTicks> launcher_show_timestamp;
};

void AppListRecordPageSwitcherSourceByEventType(ui::EventType type);

void RecordPageSwitcherSource(AppListPageSwitcherSource source);

void RecordSearchResultRemovalDialogDecision(
    SearchResultRemovalConfirmation removal_decision);

void RecordAppListUserJourneyTime(AppListShowSource source,
                                  base::TimeDelta time);

// Records metrics periodically (see interval in UserMetricsRecorder).
void RecordPeriodicAppListMetrics();

ASH_EXPORT void RecordAppListByCollectionLaunched(
    AppCollection collection,
    bool is_apps_collections_page);

ASH_EXPORT void RecordAppListAppLaunched(AppListLaunchedFrom launched_from,
                                         AppListViewState app_list_state,
                                         bool is_tablet_mode,
                                         bool app_list_shown);

ASH_EXPORT void RecordLauncherWorkflowMetrics(
    AppListUserAction action,
    bool is_tablet_mode,
    std::optional<base::TimeTicks> launcher_show_time);

ASH_EXPORT bool IsCommandIdAnAppLaunch(int command_id);

ASH_EXPORT void ReportPaginationSmoothness(int smoothness);

ASH_EXPORT void ReportCardifiedSmoothness(bool is_entering_cardified,
                                          int smoothness);

void ReportReorderAnimationSmoothness(bool in_tablet, int smoothness);

void RecordAppListSortAction(AppListSortOrder new_order, bool in_tablet);

void ReportItemDragReorderAnimationSmoothness(bool in_tablet, int smoothness);

// Invoked when the app list session ends, records metrics of interest during
// the session.
void RecordMetricsOnSessionEnd();

// Records the number of files that have been removed from the Launcher Continue
// Section in the session. This also increments the internal counter to keep
// track of the number of files that have been removed.
void RecordCumulativeContinueSectionResultRemovedNumber();

// Resets the count for the number of files that have been removed from the
// Launcher Continue Section in the session.
void ResetContinueSectionFileRemovedCountForTest();

// Records a metric for whether the user has hidden the continue section.
void RecordHideContinueSectionMetric();

// Records the number of times that the search category filter menu is opened.
void RecordSearchCategoryFilterMenuOpened();

// Records the metrics for the enable state of each search category.
void RecordSearchCategoryEnableState(
    const CategoryEnableStateMap& category_to_state);

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_METRICS_H_
