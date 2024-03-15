// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
#define ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/test/ash_test_color_generator.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/animation/tween.h"

namespace base {
class TimeDelta;
}

namespace views {
class View;
}

namespace ash {

class AppListBubbleAppsPage;
class AppListBubbleAppsCollectionsPage;
class AppListBubbleAssistantPage;
class AppListBubbleSearchPage;
class AppListBubbleView;
class AppListControllerImpl;
class AppListFolderView;
class AppListView;
class AppsContainerView;
class ContinueSectionView;
class PagedAppsGridView;
class AppListSearchView;
class RecentAppsView;
class ScrollableAppsGridView;
class SearchBoxView;
class SearchResultPageView;
class SearchResultPageAnchoredDialog;
enum class AppListViewState;

class AppListTestHelper {
 public:
  // The color types of app list item icons.
  enum class IconColorType {
    // Use the default icon color which is SK_ColorRED.
    kDefaultColor,

    // This color type guarantees that the neighboring app list items added by
    // the test helper have different icon colors.
    kAlternativeColor,

    // The icon is transparent.
    kNotSet,
  };

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
                                           base::TimeDelta duration,
                                           gfx::Tween::Type tween_type);

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

  // Adds `num_apps` to the app list model. These app items have transparent
  // icons. Their names are set so that the accessibility paint checker tests
  // pass (focusable views are expected to have accessible names).
  void AddAppItems(int num_apps);

  // Similar to `AddAppItems()` but provides the options to set item icon colors
  // and names.
  void AddAppItemsWithColorAndName(int num_apps,
                                   IconColorType color_type,
                                   bool set_name);

  // Similar to `AddAppItems()` but provides the option to set items an initial
  // collection.
  void AddAppListItemsWithCollection(AppCollection collection_id, int num_apps);

  // Adds `num_results` to continue section in the app list.
  void AddContinueSuggestionResults(int num_results);

  // Adds `num_apps` recent apps to the recent apps view.
  void AddRecentApps(int num_apps);

  // Whether the app list is showing a folder.
  bool IsInFolderView();

  // Enables/Disables the app list nudge for testing.
  void DisableAppListNudge(bool disable);

  // Accessibility helpers.
  views::View* GetAccessibilityAnnounceView();

  // Fullscreen/peeking launcher helpers.
  AppListView* GetAppListView();
  SearchBoxView* GetSearchBoxView();
  AppsContainerView* GetAppsContainerView();
  AppListFolderView* GetFullscreenFolderView();
  RecentAppsView* GetFullscreenRecentAppsView();
  ContinueSectionView* GetFullscreenContinueSectionView();
  SearchResultPageView* GetFullscreenSearchResultPageView();
  SearchResultPageAnchoredDialog* GetFullscreenSearchPageDialog();
  views::View* GetFullscreenLauncherAppsSeparatorView();

  // Whether the fullscreen/peeking launcher is showing the search results view.
  bool IsShowingFullscreenSearchResults();

  // Paged launcher helpers.
  PagedAppsGridView* GetRootPagedAppsGridView();

  // Bubble launcher helpers. The bubble must be open before calling these.
  AppListBubbleView* GetBubbleView();
  SearchBoxView* GetBubbleSearchBoxView();
  AppListFolderView* GetBubbleFolderView();
  AppListBubbleAppsPage* GetBubbleAppsPage();
  AppListBubbleAppsCollectionsPage* GetBubbleAppsCollectionsPage();
  ContinueSectionView* GetBubbleContinueSectionView();
  RecentAppsView* GetBubbleRecentAppsView();
  ScrollableAppsGridView* GetScrollableAppsGridView();
  views::View* GetAppCollectionsSectionsContainer();
  AppListBubbleSearchPage* GetBubbleSearchPage();
  SearchResultPageAnchoredDialog* GetBubbleSearchPageDialog();
  AppListBubbleAssistantPage* GetBubbleAssistantPage();
  SearchModel::SearchResults* GetSearchResults();
  views::View* GetBubbleLauncherAppsSeparatorView();
  std::vector<ash::AppListSearchResultCategory>* GetOrderedResultCategories();
  AppListSearchView* GetBubbleAppListSearchView();

  test::AppListTestModel* model() { return &model_; }
  SearchModel* search_model() { return &search_model_; }
  QuickAppAccessModel* quick_app_access_model() {
    return &quick_app_access_model_;
  }
  TestAppListClient* app_list_client() { return app_list_client_.get(); }

 private:
  // Helper function to set user prefs relative to the app_list in tests.
  void ConfigureDefaultUserPrefs();

  test::AppListTestModel model_;
  SearchModel search_model_;
  QuickAppAccessModel quick_app_access_model_;
  raw_ptr<AppListControllerImpl> app_list_controller_ = nullptr;
  std::unique_ptr<TestAppListClient> app_list_client_;

  AshTestColorGenerator icon_color_generator_{/*default_color=*/SK_ColorRED};
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_TEST_HELPER_H_
