// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_folder_view.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

class AppListFolderViewProductivityLauncherTest : public AshTestBase {
 public:
  AppListFolderViewProductivityLauncherTest() = default;
  ~AppListFolderViewProductivityLauncherTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    app_list_test_model_ = std::make_unique<test::AppListTestModel>();
    search_model_ = std::make_unique<SearchModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, app_list_test_model_.get(), search_model_.get());
  }

  base::test::ScopedFeatureList feature_list_{features::kProductivityLauncher};
  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
};

TEST_F(AppListFolderViewProductivityLauncherTest,
       ScrollViewSizeIsCappedForLargeFolders) {
  // Create a large number of apps, more than a 4 rows.
  app_list_test_model_->CreateAndPopulateFolderWithApps(30);

  // Open the app list and open the folder.
  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  views::View* folder_item = apps_grid_view->GetItemViewAt(0);
  LeftClickOn(folder_item);
  ASSERT_TRUE(helper->IsInFolderView());

  auto* folder_view = helper->GetBubbleFolderView();
  auto* scroll_view = folder_view->scroll_view_for_test();
  const int tile_height =
      folder_view->items_grid_view()->GetTotalTileSize(/*page=*/0).height();

  // The scroll view has space for at least 4 full rows, but not 5.
  EXPECT_GE(scroll_view->height(), tile_height * 4);
  EXPECT_LT(scroll_view->height(), tile_height * 5);
}

}  // namespace ash
