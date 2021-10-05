// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_helper.h"

#include <utility>

#include "ash/app_list/app_list_bubble_presenter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
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
}

AppListTestHelper::~AppListTestHelper() {
  app_list_controller_->SetClient(nullptr);
}

void AppListTestHelper::WaitUntilIdle() {
  base::RunLoop().RunUntilIdle();
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

void AppListTestHelper::CheckVisibility(bool visible) {
  EXPECT_EQ(visible, app_list_controller_->IsVisible());
  EXPECT_EQ(visible, app_list_controller_->GetTargetVisibility(absl::nullopt));
}

void AppListTestHelper::CheckState(AppListViewState state) {
  EXPECT_EQ(state, app_list_controller_->GetAppListViewState());
}

void AppListTestHelper::AddAppItems(int num_apps) {
  int num_apps_already_added =
      app_list_controller_->GetModel()->top_level_item_list()->item_count();
  for (int i = 0; i < num_apps; i++) {
    app_list_controller_->GetModel()->AddItem(std::make_unique<AppListItem>(
        /*app_id=*/base::NumberToString(i + num_apps_already_added)));
  }
}

void AppListTestHelper::AddPageBreakItem() {
  auto page_break_item = std::make_unique<AppListItem>(base::GenerateGUID());
  page_break_item->set_is_page_break(true);
  Shell::Get()->app_list_controller()->GetModel()->AddItem(
      std::move(page_break_item));
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

AppListView* AppListTestHelper::GetAppListView() {
  return app_list_controller_->presenter()->GetView();
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
  return GetAppsContainerView()->recent_apps();
}

PagedAppsGridView* AppListTestHelper::GetRootPagedAppsGridView() {
  return GetAppsContainerView()->apps_grid_view();
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

ContinueSectionView* AppListTestHelper::GetContinueSectionView() {
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

AppListBubbleAssistantPage* AppListTestHelper::GetBubbleAssistantPage() {
  return app_list_controller_->bubble_presenter_for_test()
      ->bubble_view_for_test()
      ->assistant_page_;
}

}  // namespace ash
