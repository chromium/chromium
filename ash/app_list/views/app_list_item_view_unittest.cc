// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class AppListItemViewTest : public AshTestBase {
 public:
  AppListItemViewTest() = default;
  ~AppListItemViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();

    app_list_test_model_ = std::make_unique<test::AppListTestModel>();
    search_model_ = std::make_unique<SearchModel>();
    Shell::Get()->app_list_controller()->SetActiveModel(
        /*profile_id=*/1, app_list_test_model_.get(), search_model_.get());
  }

  static views::View* GetNewInstallDot(AppListItemView* view) {
    return view->new_install_dot_;
  }

  AppListItem* CreateAppListItem(const std::string& name) {
    AppListItem* item = app_list_test_model_->CreateAndAddItem("id");
    item->SetName(name);
    return item;
  }

  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
};

// Tests with ProductivityLauncher disabled.
class AppListItemViewPeekingLauncherTest : public AppListItemViewTest {
 public:
  AppListItemViewPeekingLauncherTest() {
    feature_list_.InitAndDisableFeature(features::kProductivityLauncher);
  }
  ~AppListItemViewPeekingLauncherTest() override = default;

  base::test::ScopedFeatureList feature_list_;
};

// Tests with ProductivityLauncher enabled.
class AppListItemViewProductivityLauncherTest : public AppListItemViewTest {
 public:
  AppListItemViewProductivityLauncherTest() {
    feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~AppListItemViewProductivityLauncherTest() override = default;

  base::test::ScopedFeatureList feature_list_;
};

// Regression test for https://crbug.com/1298801
TEST_F(AppListItemViewPeekingLauncherTest,
       NewInstallDotIsNotShownForPeekingLauncher) {
  AppListItem* item = CreateAppListItem("Google Buzz");
  item->SetIsNewInstall(true);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  // The item does not have a new install dot or a new install tooltip.
  auto* apps_grid_view = helper->GetRootPagedAppsGridView();
  AppListItemView* item_view = apps_grid_view->GetItemViewAt(0);
  EXPECT_FALSE(GetNewInstallDot(item_view));
}

TEST_F(AppListItemViewProductivityLauncherTest, NewInstallDot) {
  AppListItem* item = CreateAppListItem("Google Buzz");
  ASSERT_FALSE(item->is_new_install());

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  // By default, the new install dot is not visible.
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* item_view = apps_grid_view->GetItemViewAt(0);
  views::View* new_install_dot = GetNewInstallDot(item_view);
  ASSERT_TRUE(new_install_dot);
  EXPECT_FALSE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz");

  // When the app is a new install the dot is visible and the tooltip changes.
  item->SetIsNewInstall(true);
  EXPECT_TRUE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz\nNew install");
}

}  // namespace ash
