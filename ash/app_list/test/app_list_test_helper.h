// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
#define ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/test_app_list_client.h"

namespace base {
class TimeDelta;
}

namespace views {
class View;
}

namespace ash {

class AppListBubbleAppsPage;
class AppListBubbleAssistantPage;
class AppListBubbleSearchPage;
class AppListBubbleView;
class AppListControllerImpl;
class AppListFolderView;
class AppListView;
class AppsContainerView;
class ContinueSectionView;
class PagedAppsGridView;
class ProductivityLauncherSearchView;
class RecentAppsView;
class ScrollableAppsGridView;
class SearchBoxView;
class SearchResultPageView;
class SearchResultPageAnchoredDialog;
enum class AppListViewState;

class AppListTestHelper {
 public:
  AppListTestHelper();

  AppListTestHelper(const AppListTestHelper&) = delete;
  AppListTestHelper& operator=(const AppListTestHelper&) = delete;

  ~AppListTestHelper();

  // Shows the app list on the default display.
  void ShowAppList();

  // Show the app list in |display_id|, and wait until animation finishes.
  // Note: we usually don't care about the show source in tests.
  void ShowAndRunLoop(uint64_t display_id);

  // Show the app list in |display_id|.
  void Show(uint64_t display_id);

  // Show the app list in |display_id| triggered with |show_source|, and wait
  // until animation finishes.
  void ShowAndRunLoop(uint64_t display_id, AppListShowSource show_source);

  // Dismiss the app list, and wait until animation finishes.
  void DismissAndRunLoop();

  // Dismiss the app list.
  void Dismiss();

  // Toggle the app list in |display_id|, and wait until animation finishes.
  // Note: we usually don't care about the show source in tests.
  void ToggleAndRunLoop(uint64_t display_id);

  // Toggle the app list in |display_id| triggered with |show_source|, and wait
  // until animation finishes.
  void ToggleAndRunLoop(uint64_t display_id, AppListShowSource show_source);

  // Slides a bubble apps page's component using a layer animation.
  void StartSlideAnimationOnBubbleAppsPage(views::View* view,
                                           int vertical_offset,
                                           base::TimeDelta duration);

  // Check the visibility value of the app list and its target.
  // Fails in tests if either one doesn't match |visible|.
  // DEPRECATED: Prefer to EXPECT_TRUE or EXPECT_FALSE the visibility directly,
  // so a failing test will print the line number of the expectation that
  // failed.
  void CheckVisibility(bool visible);

  // Check the current app list view state.
  void CheckState(AppListViewState state);

  // Run all pending in message loop to wait for animation to finish.
  void WaitUntilIdle();

  // If a folder view is shown, waits until the folder animations complete.
  void WaitForFolderAnimation();

  // Adds `num_apps` to the app list model.
  void AddAppItems(int num_apps);

  // Adds a page break item to the app list model.
  void AddPageBreakItem();

  // Adds `num_results` to continue section in the app list.
  void AddContinueSuggestionResults(int num_results);

  // Adds `num_apps` recent apps to the recent apps view.
  void AddRecentApps(int num_apps);

  // Whether the app list is showing a folder.
  bool IsInFolderView();

  // Enables/Disables the app list nudge for testing.
  void DisableAppListNudge(bool disable);

  // Fullscreen/peeking launcher helpers.
  AppListView* GetAppListView();
  SearchBoxView* GetSearchBoxView();
  AppsContainerView* GetAppsContainerView();
  AppListFolderView* GetFullscreenFolderView();
  RecentAppsView* GetFullscreenRecentAppsView();
  ContinueSectionView* GetFullscreenContinueSectionView();
  SearchResultPageView* GetFullscreenSearchResultPageView();
  SearchResultPageAnchoredDialog* GetFullscreenSearchPageDialog();
  ProductivityLauncherSearchView* GetProductivityLauncherSearchView();
  views::View* GetFullscreenLauncherAppsSeparatorView();

  // Paged launcher helpers.
  PagedAppsGridView* GetRootPagedAppsGridView();

  // Bubble launcher helpers. The bubble must be open before calling these.
  AppListBubbleView* GetBubbleView();
  SearchBoxView* GetBubbleSearchBoxView();
  AppListFolderView* GetBubbleFolderView();
  AppListBubbleAppsPage* GetBubbleAppsPage();
  ContinueSectionView* GetBubbleContinueSectionView();
  RecentAppsView* GetBubbleRecentAppsView();
  ScrollableAppsGridView* GetScrollableAppsGridView();
  AppListBubbleSearchPage* GetBubbleSearchPage();
  SearchResultPageAnchoredDialog* GetBubbleSearchPageDialog();
  AppListBubbleAssistantPage* GetBubbleAssistantPage();
  SearchModel::SearchResults* GetSearchResults();
  views::View* GetBubbleLauncherAppsSeparatorView();
  std::vector<ash::AppListSearchResultCategory>* GetOrderedResultCategories();

  TestAppListClient* app_list_client() { return app_list_client_.get(); }

 private:
  // Helper function to set user prefs relative to the app_list in tests.
  void ConfigureDefaultUserPrefs();

  test::AppListTestModel model_;
  SearchModel search_model_;
  AppListControllerImpl* app_list_controller_ = nullptr;
  std::unique_ptr<TestAppListClient> app_list_client_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
