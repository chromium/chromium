// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/controls/label.h"

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
    AppListItem* item = app_list_test_model_->CreateAndAddItem(name + "_id");
    item->SetName(name);
    return item;
  }

  std::unique_ptr<test::AppListTestModel> app_list_test_model_;
  std::unique_ptr<SearchModel> search_model_;
};

TEST_F(AppListItemViewTest, NewInstallDot) {
  AppListItem* item = CreateAppListItem("Google Buzz");
  ASSERT_FALSE(item->is_new_install());

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();
  ui::AXNodeData node_data;

  // By default, the new install dot is not visible.
  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* item_view = apps_grid_view->GetItemViewAt(0);
  views::View* new_install_dot = GetNewInstallDot(item_view);
  ASSERT_TRUE(new_install_dot);
  EXPECT_FALSE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz");
  item_view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      node_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
      "");

  // When the app is a new install the dot is visible and the tooltip changes.
  item->SetIsNewInstall(true);
  EXPECT_TRUE(new_install_dot->GetVisible());
  EXPECT_EQ(item_view->GetTooltipText({}), u"Google Buzz\nNew install");
  item_view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      node_data.GetStringAttribute(ax::mojom::StringAttribute::kDescription),
      "New install");
}

TEST_F(AppListItemViewTest, LabelInsetWithNewInstallDot) {
  AppListItem* long_item = CreateAppListItem("Very very very very long name");
  long_item->SetIsNewInstall(true);
  AppListItem* short_item = CreateAppListItem("Short");
  short_item->SetIsNewInstall(true);

  auto* helper = GetAppListTestHelper();
  helper->ShowAppList();

  auto* apps_grid_view = helper->GetScrollableAppsGridView();
  AppListItemView* long_item_view = apps_grid_view->GetItemViewAt(0);
  AppListItemView* short_item_view = apps_grid_view->GetItemViewAt(1);

  // The item with the long name has its title bounds left edge inset to make
  // space for the blue dot.
  EXPECT_LT(long_item_view->GetDefaultTitleBoundsForTest().x(),
            long_item_view->title()->x());

  // The item with the short name does not have the title bounds inset, because
  // there is enough space for the blue dot as-is.
  EXPECT_EQ(short_item_view->GetDefaultTitleBoundsForTest(),
            short_item_view->title()->bounds());
}

}  // namespace ash
