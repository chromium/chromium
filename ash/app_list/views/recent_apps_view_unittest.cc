// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/recent_apps_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test_app_list_client.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_item_view_grid_delegate.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/paged_apps_grid_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

// Returns the first window with type WINDOW_TYPE_MENU found via depth-first
// search. Returns nullptr if no such window exists.
aura::Window* FindMenuWindow(aura::Window* root) {
  if (root->GetType() == aura::client::WINDOW_TYPE_MENU)
    return root;
  for (aura::Window* child : root->children()) {
    auto* menu_in_child = FindMenuWindow(child);
    if (menu_in_child)
      return menu_in_child;
  }
  return nullptr;
}

}  // namespace

// Parameterized to test recent apps in the app list bubble and tablet mode.
class RecentAppsViewTest : public AshTestBase,
                           public testing::WithParamInterface<bool> {
 public:
  RecentAppsViewTest() = default;
  ~RecentAppsViewTest() override = default;

  // Whether we should run the test in tablet mode.
  bool tablet_mode_param() { return GetParam(); }

  void ShowAppList() {
    if (tablet_mode_param()) {
      Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
      test_api_ = std::make_unique<test::AppsGridViewTestApi>(
          GetAppListTestHelper()->GetRootPagedAppsGridView());
    } else {
      Shell::Get()->app_list_controller()->ShowAppList(
          AppListShowSource::kSearchKey);
      test_api_ = std::make_unique<test::AppsGridViewTestApi>(
          GetAppListTestHelper()->GetScrollableAppsGridView());
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

  void AddAppListItem(AppListModel* model, const std::string& id) {
    AppListItem* item = model->AddItem(std::make_unique<AppListItem>(id));

    // Give each item a name so that the accessibility paint checks pass.
    // (Focusable items should have accessible names.)
    model->SetItemName(item, item->id());
  }

  void AddAppListItem(const std::string& id) {
    AddAppListItem(AppListModelProvider::Get()->model(), id);
  }

  void AddSearchResult(SearchModel* model,
                       const std::string& id,
                       AppListSearchResultType type) {
    auto result = std::make_unique<TestSearchResult>();
    result->set_result_id(id);
    result->set_result_type(type);
    result->set_display_type(SearchResultDisplayType::kRecentApps);
    model->results()->Add(std::move(result));
  }

  void AddSearchResult(const std::string& id, AppListSearchResultType type) {
    AddSearchResult(AppListModelProvider::Get()->search_model(), id, type);
  }

  // Adds `count` installed app search results.
  void AddAppResults(int count) {
    for (int i = 0; i < count; ++i) {
      std::string id = base::StringPrintf("id%d", i);
      AddAppListItem(id);
      AddSearchResult(id, AppListSearchResultType::kInstalledApp);
    }
  }

  void RemoveApp(const std::string& id) {
    AppListModelProvider::Get()->model()->DeleteItem(id);
  }

  std::vector<AppListItemView*> GetAppListItemViews() {
    std::vector<AppListItemView*> views;
    RecentAppsView* recent_apps = GetRecentAppsView();
    for (int i = 0; i < recent_apps->GetItemViewCount(); i++)
      views.push_back(recent_apps->GetItemViewAt(i));
    return views;
  }

  std::vector<std::string> GetRecentAppsIds() {
    std::vector<AppListItemView*> views = GetAppListItemViews();
    std::vector<std::string> ids;
    for (auto* view : views)
      ids.push_back(view->item()->id());
    return ids;
  }

 protected:
  AppListItemView::DragState GetDragState(AppListItemView* view) {
    return view->drag_state_;
  }

  std::unique_ptr<test::AppsGridViewTestApi> test_api_;
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

TEST_P(RecentAppsViewTest, IgnoreResultsNotInAppListModel) {
  // Result without an associated app list item.
  AddSearchResult("id1", AppListSearchResultType::kInstalledApp);

  AddAppListItem("id2");
  AddSearchResult("id2", AppListSearchResultType::kPlayStoreApp);
  AddAppListItem("id3");
  AddSearchResult("id3", AppListSearchResultType::kInstantApp);
  AddAppListItem("id4");
  AddSearchResult("id4", AppListSearchResultType::kInternalApp);
  AddAppListItem("id5");
  AddSearchResult("id5", AppListSearchResultType::kInternalApp);
  AddAppListItem("id6");
  AddSearchResult("id6", AppListSearchResultType::kInternalApp);

  // Verify that recent apps UI does not leave an empty space for results that
  // are not present in app list model.
  ShowAppList();

  EXPECT_EQ(std::vector<std::string>({"id2", "id3", "id4", "id5", "id6"}),
            GetRecentAppsIds());
}

TEST_P(RecentAppsViewTest, ItemsMatchGridWith5Items) {
  AddAppResults(5);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(5u, items.size());

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(test_api_->GetViewAtVisualIndex(0, i)->x(),
              items[i]->bounds().x());
    EXPECT_EQ(test_api_->GetViewAtVisualIndex(0, i)->bounds().right(),
              items[i]->bounds().right());
  }
}

TEST_P(RecentAppsViewTest, ItemsMatchGridWith4Items) {
  AddAppResults(4);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(4u, items.size());

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(test_api_->GetViewAtVisualIndex(0, i)->x(),
              items[i]->bounds().x());
    EXPECT_EQ(test_api_->GetViewAtVisualIndex(0, i)->bounds().right(),
              items[i]->bounds().right());
  }
}

TEST_P(RecentAppsViewTest, IsEmptyWithLessThan4Results) {
  AddAppResults(3);
  ShowAppList();

  EXPECT_EQ(GetAppListItemViews().size(), 0u);
}

TEST_P(RecentAppsViewTest, DoesNotCreateIconsForNonApps) {
  AddSearchResult("id1", AppListSearchResultType::kAnswerCard);
  AddSearchResult("id2", AppListSearchResultType::kAssistantText);

  ShowAppList();

  EXPECT_EQ(GetAppListItemViews().size(), 0u);
}

TEST_P(RecentAppsViewTest, DoesNotCreateIconForMismatchedId) {
  AddAppResults(4);
  AddAppListItem("id");
  AddSearchResult("bad id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  RecentAppsView* view = GetRecentAppsView();
  EXPECT_EQ(view->children().size(), 4u);
}

TEST_P(RecentAppsViewTest, ClickOrTapOnRecentApp) {
  AddAppResults(4);
  AddAppListItem("id");
  AddSearchResult("id", AppListSearchResultType::kInstalledApp);

  ShowAppList();

  // Click or tap on the first icon.
  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_FALSE(items.empty());
  views::View* icon = items.back();

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
  AddAppResults(4);
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
  // Show an app list with 4 recent apps.
  AddAppResults(4);
  ShowAppList();

  std::vector<AppListItemView*> items = GetAppListItemViews();
  ASSERT_EQ(4u, items.size());
  AppListItemView* item1 = items[0];
  AppListItemView* item2 = items[1];

  // The grid delegates are the same, so it doesn't matter which one we use for
  // expectations below.
  ASSERT_EQ(item1->grid_delegate_for_test(), item2->grid_delegate_for_test());
  AppListItemViewGridDelegate* grid_delegate = item1->grid_delegate_for_test();

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

TEST_P(RecentAppsViewTest, UpdateAppsOnModelChange) {
  AddAppResults(5);
  ShowAppList();

  // Verify initial set of shown apps.
  EXPECT_EQ(std::vector<std::string>({"id0", "id1", "id2", "id3", "id4"}),
            GetRecentAppsIds());

  // Update active model, and make sure the recent apps view gets updated
  // accordingly.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  auto quick_app_access_model_override =
      std::make_unique<QuickAppAccessModel>();

  for (int i = 0; i < 4; ++i) {
    const std::string id = base::StringPrintf("other_id%d", i);
    AddAppListItem(model_override.get(), id);
    AddSearchResult(search_model_override.get(), id,
                    AppListSearchResultType::kInstalledApp);
  }

  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get(),
      quick_app_access_model_override.get());
  GetRecentAppsView()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(std::vector<std::string>(
                {"other_id0", "other_id1", "other_id2", "other_id3"}),
            GetRecentAppsIds());

  // Tap an item and make sure the item activation is recorded.
  GetEventGenerator()->GestureTapAt(
      GetAppListItemViews()[1]->GetBoundsInScreen().CenterPoint());

  // The item was activated.
  EXPECT_EQ(1, GetTestAppListClient()->activate_item_count());
  EXPECT_EQ("other_id1", GetTestAppListClient()->activate_item_last_id());

  // Recent apps should be cleared if app list models get reset.
  Shell::Get()->app_list_controller()->ClearActiveModel();
  EXPECT_EQ(std::vector<std::string>{}, GetRecentAppsIds());
}

TEST_P(RecentAppsViewTest, VisibleWithMinimumApps) {
  AddAppResults(4);
  ShowAppList();

  // Verify the visibility of the recent_apps section.
  EXPECT_TRUE(GetRecentAppsView()->GetVisible());
}

TEST_P(RecentAppsViewTest, NotVisibleWithLessThanMinimumApps) {
  AddAppResults(3);
  ShowAppList();

  // Verify the visibility of the recent_apps section.
  EXPECT_FALSE(GetRecentAppsView()->GetVisible());
}

TEST_P(RecentAppsViewTest, RemoveAppRemovesFromRecentApps) {
  AddAppResults(5);
  ShowAppList();

  // Verify initial set of shown apps.
  EXPECT_EQ(std::vector<std::string>({"id0", "id1", "id2", "id3", "id4"}),
            GetRecentAppsIds());

  // Uninstall the first app.
  RemoveApp("id0");

  // Verify the visibility of the recent_apps section.
  EXPECT_TRUE(GetRecentAppsView()->GetVisible());
  // Verify shown apps.
  EXPECT_EQ(std::vector<std::string>({"id1", "id2", "id3", "id4"}),
            GetRecentAppsIds());
}

TEST_P(RecentAppsViewTest, RemoveAppUpdatesRecentAppsWithOtherApps) {
  AddAppResults(6);
  ShowAppList();

  // Verify initial set of shown apps.
  EXPECT_EQ(std::vector<std::string>({"id0", "id1", "id2", "id3", "id4"}),
            GetRecentAppsIds());

  // Uninstall the first app.
  RemoveApp("id0");

  // Verify the visibility of the recent_apps section.
  EXPECT_TRUE(GetRecentAppsView()->GetVisible());
  // Verify shown apps.
  EXPECT_EQ(std::vector<std::string>({"id1", "id2", "id3", "id4", "id5"}),
            GetRecentAppsIds());
}

TEST_P(RecentAppsViewTest, RemoveAppsRemovesFromRecentAppsUntilHides) {
  AddAppResults(5);
  ShowAppList();

  // Verify initial set of shown apps.
  EXPECT_EQ(std::vector<std::string>({"id0", "id1", "id2", "id3", "id4"}),
            GetRecentAppsIds());

  // Uninstall the first app.
  RemoveApp("id0");

  // Verify the visibility of the recent_apps section.
  EXPECT_TRUE(GetRecentAppsView()->GetVisible());
  // Verify shown apps.
  EXPECT_EQ(std::vector<std::string>({"id1", "id2", "id3", "id4"}),
            GetRecentAppsIds());

  // Uninstall another app.
  RemoveApp("id1");

  // Verify the visibility of the recent_apps section.
  EXPECT_FALSE(GetRecentAppsView()->GetVisible());
}

TEST_P(RecentAppsViewTest, AttemptTouchDragRecentApp) {
  AddAppResults(5);
  ShowAppList();

  AppListItemView* view = GetAppListItemViews()[0];
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  auto* generator = GetEventGenerator();
  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveTouch(from);
  generator->PressTouch();

  // Attempt to fire the touch drag timer. Recent apps view should not trigger
  // the timer.
  EXPECT_FALSE(view->FireTouchDragTimerForTest());

  // Verify the apps did not enter dragged state.
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_TRUE(view->title()->GetVisible());
}

TEST_P(RecentAppsViewTest, AttemptMouseDragRecentApp) {
  AddAppResults(5);
  ShowAppList();

  AppListItemView* view = GetAppListItemViews()[0];
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);

  auto* generator = GetEventGenerator();
  gfx::Point from = view->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(from);
  generator->PressLeftButton();

  // Attempt to fire the mouse drag timer. Recent apps view should not trigger
  // the timer.
  EXPECT_FALSE(view->FireMouseDragTimerForTest());

  // Verify the apps did not enter dragged state.
  EXPECT_EQ(GetDragState(view), AppListItemView::DragState::kNone);
  EXPECT_TRUE(view->title()->GetVisible());
}

}  // namespace ash
