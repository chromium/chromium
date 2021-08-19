// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/paged_apps_grid_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

class PagedAppsGridViewTest : public AshTestBase {
 public:
  PagedAppsGridViewTest() {
    scoped_features_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~PagedAppsGridViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    grid_test_api_ = std::make_unique<test::AppsGridViewTestApi>(
        GetAppListTestHelper()->GetRootPagedAppsGridView());
  }

  std::unique_ptr<test::AppsGridViewTestApi> grid_test_api_;
  base::test::ScopedFeatureList scoped_features_;
};

// Test that the first page of the root level paged apps grid view holds 15 apps
// while subsequent pages can hold up to 20.
TEST_F(PagedAppsGridViewTest, PageMaxAppCounts) {
  GetAppListTestHelper()->AddAppItems(40);

  // There should be a total of 40 items in the item list.
  AppListItemList* item_list =
      Shell::Get()->app_list_controller()->GetModel()->top_level_item_list();
  ASSERT_EQ(40u, item_list->item_count());

  // The first page should be maxed at 15 apps, the second page maxed at 20
  // apps, and the third page should hold the leftover 5 apps totalling to 40
  // apps.
  EXPECT_EQ(15, grid_test_api_->AppsOnPage(0));
  EXPECT_EQ(20, grid_test_api_->AppsOnPage(1));
  EXPECT_EQ(5, grid_test_api_->AppsOnPage(2));
}

}  // namespace
}  // namespace ash
