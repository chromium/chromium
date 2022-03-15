// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_helper.h"

#include <utility>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/continue_section_view.h"
#include "ash/app_list/views/productivity_launcher_search_view.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

AppListTestHelper::AppListTestHelper() {
  // The app list controller is ready after Shell is created.
  app_list_controller_ = Shell::Get()->app_list_controller();
  DCHECK(app_list_controller_);

  // Use a new app list client for each test
  app_list_client_ = std::make_unique<TestAppListClient>();
  app_list_controller_->SetClient(app_list_client_.get());
  app_list_controller_->SetActiveModel(/*profile_id=*/1, &model_,
                                       &search_model_);
  // Disable app list nudge as default.
  DisableAppListNudge(true);
  AppListNudgeController::SetPrivacyNoticeAcceptedForTest(true);
}

AppListTestHelper::~AppListTestHelper() {
  app_list_controller_->ClearActiveModel();
  app_list_controller_->SetClient(nullptr);
}

void AppListTestHelper::WaitUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void AppListTestHelper::WaitForFolderAnimation() {
  AppListFolderView* folder_view = nullptr;
  if (!Shell::Get()->IsInTabletMode() &&
      features::IsProductivityLauncherEnabled()) {
    folder_view = GetBubbleFolderView();
  } else {
    folder_view = GetFullscreenFolderView();
  }
  if (!folder_view || !folder_view->IsAnimationRunning())
    return;

  base::RunLoop run_loop;
  folder_view->SetAnimationDoneTestCallback(run_loop.QuitClosure());
  run_loop.Run();
}

void AppListTestHelper::ShowAppList() {
  app_list_controller_->ShowAppList();
}

void AppListTestHelper::ShowAndRunLoop(uint64_t display_id) {
  ShowAndRunLoop(display_id, AppListShowSource::kSearchKey);
}

void AppListTestHelper::Show(uint64_t display_id) {
  ShowAndRunLoop(display_id, AppListShowSource::kSearchKey);
}

void AppListTestHelper::ShowAndRunLoop(uint64_t display_id,
                                       AppListShowSource show_source) {
  app_list_controller_->Show(display_id, show_source, base::TimeTicks());
  WaitUntilIdle();
}

void AppListTestHelper::DismissAndRunLoop() {
  app_list_controller_->DismissAppList();
  WaitUntilIdle();
}

void AppListTestHelper::Dismiss() {
  app_list_controller_->DismissAppList();
}

void AppListTestHelper::ToggleAndRunLoop(uint64_t display_id) {
  ToggleAndRunLoop(display_id, AppListShowSource::kSearchKey);
}

void AppListTestHelper::ToggleAndRunLoop(uint64_t display_id,
                                         AppListShowSource show_source) {
  app_list_controller_->ToggleAppList(display_id, show_source,
                                      base::TimeTicks());
  WaitUntilIdle();
}

void AppListTestHelper::StartSlideAnimationOnBubbleAppsPage(
    views::View* view,
    int vertical_offset,
    base::TimeDelta duration) {
  GetBubbleAppsPage()->SlideViewIntoPosition(view, vertical_offset, duration);
}

void AppListTestHelper::CheckVisibility(bool visible) {
  EXPECT_EQ(visible, app_list_controller_->IsVisible());
  EXPECT_EQ(visible, app_list_controller_->GetTargetVisibility(absl::nullopt));
}

void AppListTestHelper::CheckState(AppListViewState state) {
  EXPECT_EQ(state, app_list_controller_->GetAppListViewState());
}

void AppListTestHelper::AddAppItems(int num_apps) {
  AppListModel* const model = AppListModelProvider::Get()->model();
  const int num_apps_already_added = model->top_level_item_list()->item_count();
  for (int i = 0; i < num_apps; i++) {
    model->AddItem(std::make_unique<AppListItem>(
        test::AppListTestModel::GetItemName(i + num_apps_already_added)));
  }
}

void AppListTestHelper::AddPageBreakItem() {
  auto page_break_item = std::make_unique<AppListItem>(base::GenerateGUID());
  page_break_item->set_is_page_break(true);
  AppListModelProvider::Get()->model()->AddItem(std::move(page_break_item));
}

void AppListTestHelper::AddContinueSuggestionResults(int num_results) {
  for (int i = 0; i < num_results; i++) {
    auto result = std::make_unique<TestSearchResult>();
    result->set_result_id(base::NumberToString(i));
    result->set_result_type(AppListSearchResultType::kFileChip);
    result->set_display_type(SearchResultDisplayType::kContinue);
    GetSearchResults()->Add(std::move(result));
  }
}

void AppListTestHelper::AddRecentApps(int num_apps) {
  for (int i = 0; i < num_apps; i++) {
    auto result = std::make_unique<TestSearchResult>();
    // Use the same "Item #" convention as AppListTestModel uses. The search
    // result IDs must match app item IDs in the app list data model.
    result->set_result_id(test::AppListTestModel::GetItemName(i));
    result->set_result_type(AppListSearchResultType::kInstalledApp);
    result->set_display_type(SearchResultDisplayType::kRecentApps);
    GetSearchResults()->Add(std::move(result));
  }
}

bool AppListTestHelper::IsInFolderView() {
  if (!Shell::Get()->IsInTabletMode() &&
      features::IsProductivityLauncherEnabled()) {
    return GetBubbleView()->showing_folder_for_test();
  }
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->apps_container_view()
      ->IsInFolderView();
}

void AppListTestHelper::DisableAppListNudge(bool disable) {
  AppListNudgeController::SetReorderNudgeDisabledForTest(disable);
}

AppListView* AppListTestHelper::GetAppListView() {
  return app_list_controller_->fullscreen_presenter()->GetView();
}

SearchBoxView* AppListTestHelper::GetSearchBoxView() {
  return GetAppListView()->search_box_view();
}

AppsContainerView* AppListTestHelper::GetAppsContainerView() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->apps_container_view();
}

AppListFolderView* AppListTestHelper::GetFullscreenFolderView() {
  return GetAppsContainerView()->app_list_folder_view();
}

RecentAppsView* AppListTestHelper::GetFullscreenRecentAppsView() {
  return GetAppsContainerView()->GetRecentApps();
}

ContinueSectionView* AppListTestHelper::GetFullscreenContinueSectionView() {
  return GetAppsContainerView()->GetContinueSection();
}

PagedAppsGridView* AppListTestHelper::GetRootPagedAppsGridView() {
  return GetAppsContainerView()->apps_grid_view();
}

views::View* AppListTestHelper::GetFullscreenLauncherAppsSeparatorView() {
  return GetAppsContainerView()->separator();
}

SearchResultPageView* AppListTestHelper::GetFullscreenSearchResultPageView() {
  return GetAppListView()
      ->app_list_main_view()
      ->contents_view()
      ->search_result_page_view();
}

SearchResultPageAnchoredDialog*
AppListTestHelper::GetFullscreenSearchPageDialog() {
  return GetFullscreenSearchResultPageView()->dialog_for_test();
}

AppListBubbleView* AppListTestHelper::GetBubbleView() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test();
}

SearchBoxView* AppListTestHelper::GetBubbleSearchBoxView() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->search_box_view_;
}

AppListFolderView* AppListTestHelper::GetBubbleFolderView() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->folder_view_;
}

AppListBubbleAppsPage* AppListTestHelper::GetBubbleAppsPage() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->apps_page_;
}

ContinueSectionView* AppListTestHelper::GetBubbleContinueSectionView() {
  return GetBubbleAppsPage()->continue_section_;
}

RecentAppsView* AppListTestHelper::GetBubbleRecentAppsView() {
  return GetBubbleAppsPage()->recent_apps_;
}

ScrollableAppsGridView* AppListTestHelper::GetScrollableAppsGridView() {
  return GetBubbleAppsPage()->scrollable_apps_grid_view_;
}

AppListBubbleSearchPage* AppListTestHelper::GetBubbleSearchPage() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->search_page_;
}

SearchResultPageAnchoredDialog* AppListTestHelper::GetBubbleSearchPageDialog() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->search_page_dialog_controller_->dialog();
}
AppListBubbleAssistantPage* AppListTestHelper::GetBubbleAssistantPage() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->assistant_page_;
}

SearchModel::SearchResults* AppListTestHelper::GetSearchResults() {
  return AppListModelProvider::Get()->search_model()->results();
}

std::vector<ash::AppListSearchResultCategory>*
AppListTestHelper::GetOrderedResultCategories() {
  return AppListModelProvider::Get()->search_model()->ordered_categories();
}

ProductivityLauncherSearchView*
AppListTestHelper::GetProductivityLauncherSearchView() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->search_page_->search_view();
}

views::View* AppListTestHelper::GetBubbleLauncherAppsSeparatorView() {
  return GetBubbleAppsPage()->separator_for_test();
}

}  // namespace ash
