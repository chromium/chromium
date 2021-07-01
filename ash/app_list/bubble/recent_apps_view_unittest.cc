// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/recent_apps_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Returns the first window with type WINDOW_TYPE_MENU found via depth-first
// search. Returns nullptr if no such window exists.
aura::Window* FindMenuWindow(aura::Window* root) {
  if (root->GetType() == aura::client::WINDOW_TYPE_MENU)
    return root;
  for (auto* child : root->children()) {
    auto* menu_in_child = FindMenuWindow(child);
    if (menu_in_child)
      return menu_in_child;
  }
  return nullptr;
}

void AddAppListItem(const std::string& id) {
  Shell::Get()->app_list_controller()->GetModel()->AddItem(
      std::make_unique<AppListItem>(id));
}

void AddSearchResult(const std::string& id, AppListSearchResultType type) {
  auto result = std::make_unique<TestSearchResult>();
  result->set_result_id(id);
  result->set_result_type(type);
  // TODO(crbug.com/1216662): Replace with a real display type after the ML team
  // gives us a way to query directly for recent apps.
  result->set_display_type(SearchResultDisplayType::kChip);
  Shell::Get()->app_list_controller()->GetSearchModel()->results()->Add(
      std::move(result));
}

void ShowAppList() {
  Shell::Get()->app_list_controller()->ShowAppList();
}

class RecentAppsViewTest : public AshTestBase {
 public:
  RecentAppsViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kAppListBubble);
  }
  ~RecentAppsViewTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->app_list_controller()->SetClient(&app_list_client_);
  }

  void RightClickOn(views::View* view) {
    GetEventGenerator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickRightButton();
  }

  RecentAppsView* GetRecentAppsView() {
    return GetAppListTestHelper()->GetBubbleRecentAppsView();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  TestAppListClient app_list_client_;
};

TEST_F(RecentAppsViewTest, CreatesIconsForApps) {
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  AddAppListItem("id2");
  AddSearchResult("id2", AppListSearchResultType::kPlayStoreApp);
  AddAppListItem("id3");
  AddSearchResult("id3", AppListSearchResultType::kInstantApp);
  AddAppListItem("id4");
  AddSearchResult("id4", AppListSearchResultType::kInternalApp);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 4u);
}

TEST_F(RecentAppsViewTest, DoesNotCreateIconsForNonApps) {
  AddSearchResult("id1", AppListSearchResultType::kAnswerCard);
  AddSearchResult("id2", AppListSearchResultType::kFileChip);
  AddSearchResult("id3", AppListSearchResultType::kAssistantText);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 0u);
}

TEST_F(RecentAppsViewTest, DoesNotCreateIconForMismatchedId) {
  AddAppListItem("id");
  AddSearchResult("bad id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 0u);
}

TEST_F(RecentAppsViewTest, ClickOnRecentApp) {
  AddAppListItem("id");
  AddSearchResult("id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  // Click on the first icon.
  RecentAppsView* view = GetRecentAppsView();
  ASSERT_FALSE(view->children().empty());
  views::View* icon = view->children()[0];
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  EXPECT_EQ(1, app_list_client_.activate_item_count());
  EXPECT_EQ("id", app_list_client_.activate_item_last_id());
}

TEST_F(RecentAppsViewTest, RightClickOpensContextMenu) {
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  ShowAppList();

  // Right click on the first icon.
  RecentAppsView* view = GetRecentAppsView();
  ASSERT_FALSE(view->children().empty());
  views::View* icon = view->children()[0];
  GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickRightButton();

  // A menu opened.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::Window* menu = FindMenuWindow(root);
  ASSERT_TRUE(menu);

  // The menu is on screen.
  gfx::Rect root_bounds = root->GetBoundsInScreen();
  gfx::Rect menu_bounds = menu->GetBoundsInScreen();
  EXPECT_TRUE(root_bounds.Contains(menu_bounds));
}

TEST_F(RecentAppsViewTest, AppIconSelectedWhenMenuIsShown) {
  // Show an app list with 2 recent apps.
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  AddAppListItem("id2");
  AddSearchResult("id2", AppListSearchResultType::kInstalledApp);
  ShowAppList();

  // There are 2 items.
  RecentAppsView* view = GetRecentAppsView();
  ASSERT_EQ(2u, view->children().size());
  AppListItemView* item1 = view->GetItemViewForTest(0);
  AppListItemView* item2 = view->GetItemViewForTest(1);

  // The grid delegates are the same, so it doesn't matter which one we use for
  // expectations below.
  ASSERT_EQ(item1->grid_delegate_for_test(), item2->grid_delegate_for_test());
  AppListItemView::GridDelegate* grid_delegate =
      item1->grid_delegate_for_test();

  // Right clicking an item selects it.
  RightClickOn(item1);
  EXPECT_TRUE(grid_delegate->IsSelectedView(item1));
  EXPECT_FALSE(grid_delegate->IsSelectedView(item2));

  // Second click closes the menu.
  RightClickOn(item1);
  EXPECT_FALSE(grid_delegate->IsSelectedView(item1));
  EXPECT_FALSE(grid_delegate->IsSelectedView(item2));

  // Right clicking the other item selects it.
  RightClickOn(item2);
  EXPECT_FALSE(grid_delegate->IsSelectedView(item1));
  EXPECT_TRUE(grid_delegate->IsSelectedView(item2));

  item2->CancelContextMenu();
  EXPECT_FALSE(grid_delegate->IsSelectedView(item1));
  EXPECT_FALSE(grid_delegate->IsSelectedView(item2));
}

}  // namespace
}  // namespace ash
