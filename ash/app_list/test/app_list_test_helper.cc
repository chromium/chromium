// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_helper.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/shell.h"
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
  EXPECT_EQ(visible, app_list_controller_->GetTargetVisibility());
}

void AppListTestHelper::CheckState(ash::AppListViewState state) {
  EXPECT_EQ(state, app_list_controller_->GetAppListViewState());
}

AppListView* AppListTestHelper::GetAppListView() {
  return app_list_controller_->presenter()->GetView();
}

}  // namespace ash
