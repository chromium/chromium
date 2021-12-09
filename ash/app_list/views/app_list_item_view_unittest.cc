// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class AppListItemViewProductivityLauncherTest : public AshTestBase {
 public:
  AppListItemViewProductivityLauncherTest() = default;
  ~AppListItemViewProductivityLauncherTest() override = default;

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

  static void SetName(AppListItem* item, const std::string& name) {
    item->SetName(name);
  }

  base::test::ScopedFeatureList feature_list_{features::kProductivityLauncher};
  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
};

TEST_F(AppListItemViewProductivityLauncherTest, NewInstallDot) {
  auto* item = app_list_test_model_->CreateAndAddItem("id");
  SetName(item, "Google Buzz");
  ASSERT_FALSE(item->is_new_install());

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  // By default, the new install dot is not visible.
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* item_view = apps_grid_view->GetItemViewAt(0);
  views::View* new_install_dot = GetNewInstallDot(item_view);
  EXPECT_FALSE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz");

  // When the app is a new install the dot is visible and the tooltip changes.
  item->SetIsNewInstall(true);
  EXPECT_TRUE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz\nNew install");
}

}  // namespace ash
