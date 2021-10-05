// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/recent_apps_view.h"

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
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {
namespace {

// Used to compare distances between point that allow for certain margin of
// error when comparing horizontal distance. Used to compare spacing between
// views that accounts for 1 off rounding errors.
bool AreVectorsClose(const gfx::Vector2d& v1, const gfx::Vector2d& v2) {
  const int kHorizontalMarginOfError = 1;
  return std::abs(v1.x() - v2.x()) <= kHorizontalMarginOfError &&
         std::abs(v1.y() - v2.y()) == 0;
}

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

// Parameterized to test recent apps in the app list bubble and tablet mode.
class RecentAppsViewTest : public AshTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  RecentAppsViewTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~RecentAppsViewTest() override = default;

  // Whether we should run the test in tablet mode.
  bool tablet_mode_param() { return GetParam(); }

  void ShowAppList() {
    if (tablet_mode_param()) {
      Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    } else {
      Shell::Get()->app_list_controller()->ShowAppList();
    }
  }

  void RightClickOn(views::View* view) {
    GetEventGenerator()->MoveMouseTo(view->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickRightButton();
  }

  RecentAppsView* GetRecentAppsView() {
    if (tablet_mode_param())
      return GetAppListTestHelper()->GetFullscreenRecentAppsView();
    return GetAppListTestHelper()->GetBubbleRecentAppsView();
  }

  // Adds `count` installed app search results.
  void AddAppResults(int count) {
    for (int i = 0; i < count; ++i) {
      std::string id = base::StringPrintf("id%d", i);
      AddAppListItem(id);
      AddSearchResult(id, AppListSearchResultType::kInstalledApp);
    }
  }

  std::vector<AppListItemView*> GetAppListItemViews() {
    std::vector<AppListItemView*> views;
    RecentAppsView* recent_apps = GetRecentAppsView();
    for (int i = 0; i < recent_apps->GetItemViewCount(); i++)
      views.push_back(recent_apps->GetItemViewAt(i));
    return views;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};
INSTANTIATE_TEST_SUITE_P(All, RecentAppsViewTest, testing::Bool());

TEST_P(RecentAppsViewTest, CreatesIconsForApps) {
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  AddAppListItem("id2");
  AddSearchResult("id2", AppListSearchResultType::kPlayStoreApp);
  AddAppListItem("id3");
  AddSearchResult("id3", AppListSearchResultType::kInstantApp);
  AddAppListItem("id4");
  AddSearchResult("id4", AppListSearchResultType::kInternalApp);

  ShowAppList();

  EXPECT_EQ(GetAppListItemViews().size(), 4u);
}

TEST_P(RecentAppsViewTest, ItemsEvenlySpacedInTheViewWith5Items) {
  AddAppResults(5);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(5u, items.size());

  for (int i = 4; i > 1; --i) {
    const gfx::Vector2d right_space = items[i]->bounds().left_center() -
                                      items[i - 1]->bounds().right_center();
    const gfx::Vector2d left_space = items[i - 1]->bounds().left_center() -
                                     items[i - 2]->bounds().right_center();
    EXPECT_TRUE(AreVectorsClose(right_space, left_space))
        << i << " " << right_space.ToString() << " " << left_space.ToString();
  }
}

TEST_P(RecentAppsViewTest, ResultItemsCoverWholeContainerWith5Items) {
  AddAppResults(5);
  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(5u, items.size());

  EXPECT_EQ(view->GetContentsBounds().left_center(),
            items[0]->bounds().left_center());
  EXPECT_EQ(view->GetContentsBounds().right_center(),
            items[4]->bounds().right_center());
}

TEST_P(RecentAppsViewTest, ItemsEvenlySpacedInTheViewWith4Items) {
  AddAppResults(4);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(4u, items.size());

  for (int i = 3; i > 1; --i) {
    const gfx::Vector2d right_space = items[i]->bounds().left_center() -
                                      items[i - 1]->bounds().right_center();
    const gfx::Vector2d left_space = items[i - 1]->bounds().left_center() -
                                     items[i - 2]->bounds().right_center();
    EXPECT_TRUE(AreVectorsClose(right_space, left_space))
        << i << " " << right_space.ToString() << " " << left_space.ToString();
  }
}

TEST_P(RecentAppsViewTest, ResultItemsCoverWholeContainerWith4Items) {
  AddAppResults(4);
  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(4u, items.size());

  EXPECT_EQ(view->GetContentsBounds().left_center(),
            items[0]->bounds().left_center());
  EXPECT_EQ(view->GetContentsBounds().right_center(),
            items[3]->bounds().right_center());
}

TEST_P(RecentAppsViewTest, ItemsEvenlySpacedInTheViewWith3Items) {
  AddAppResults(3);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(3u, items.size());

  for (int i = 2; i > 1; --i) {
    const gfx::Vector2d right_space = items[i]->bounds().left_center() -
                                      items[i - 1]->bounds().right_center();
    const gfx::Vector2d left_space = items[i - 1]->bounds().left_center() -
                                     items[i - 2]->bounds().right_center();
    EXPECT_TRUE(AreVectorsClose(right_space, left_space))
        << i << " " << right_space.ToString() << " " << left_space.ToString();
  }
}

TEST_P(RecentAppsViewTest, ResultItemsCoverWholeContainerWith3Items) {
  AddAppResults(3);
  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(3u, items.size());

  EXPECT_EQ(view->GetContentsBounds().left_center(),
            items[0]->bounds().left_center());
  EXPECT_EQ(view->GetContentsBounds().right_center(),
            items[2]->bounds().right_center());
}

TEST_P(RecentAppsViewTest, DoesNotCreateIconsForNonApps) {
  AddSearchResult("id1", AppListSearchResultType::kAnswerCard);
  AddSearchResult("id2", AppListSearchResultType::kFileChip);
  AddSearchResult("id3", AppListSearchResultType::kAssistantText);

  ShowAppList();

  EXPECT_EQ(GetAppListItemViews().size(), 0u);
}

TEST_P(RecentAppsViewTest, DoesNotCreateIconForMismatchedId) {
  AddAppListItem("id");
  AddSearchResult("bad id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 0u);
}

TEST_P(RecentAppsViewTest, ClickOrTapOnRecentApp) {
  AddAppListItem("id");
  AddSearchResult("id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  // Click or tap on the first icon.
  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_FALSE(items.empty());
  views::View* icon = items[0];

  if (tablet_mode_param()) {
    // Tap an item and make sure the item activation is recorded.
    GetEventGenerator()->GestureTapAt(icon->GetBoundsInScreen().CenterPoint());
  } else {
    GetEventGenerator()->MoveMouseTo(icon->GetBoundsInScreen().CenterPoint());
    GetEventGenerator()->ClickLeftButton();
  }

  // The item was activated.
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_count());
  EXPECT_EQ("id", GetTestAppListClient()->activate_item_last_id());
}

TEST_P(RecentAppsViewTest, RightClickOpensContextMenu) {
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  ShowAppList();

  // Right click on the first icon.
  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_FALSE(items.empty());
  GetEventGenerator()->MoveMouseTo(items[0]->GetBoundsInScreen().CenterPoint());
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

TEST_P(RecentAppsViewTest, AppIconSelectedWhenMenuIsShown) {
  // Show an app list with 2 recent apps.
  AddAppListItem("id1");
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);
  AddAppListItem("id2");
  AddSearchResult("id2", AppListSearchResultType::kInstalledApp);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(2u, items.size());
  AppListItemView* item1 = items[0];
  AppListItemView* item2 = items[1];

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
