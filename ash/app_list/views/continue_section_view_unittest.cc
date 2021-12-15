// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {
namespace {

using test::AppListTestViewDelegate;

void AddSearchResultToModel(const std::string& id,
                            AppListSearchResultType type,
                            SearchModel* model,
                            const std::string& title) {
  auto result = std::make_unique<TestSearchResult>();
  result->set_result_id(id);
  result->set_title(base::ASCIIToUTF16(title));
  result->set_result_type(type);
  result->set_display_type(SearchResultDisplayType::kContinue);
  model->results()->Add(std::move(result));
}

class ContinueSectionViewTestBase : public AshTestBase {
 public:
  explicit ContinueSectionViewTestBase(bool tablet_mode)
      : tablet_mode_(tablet_mode) {
    scoped_feature_list_.InitAndEnableFeature(features::kProductivityLauncher);
  }
  ~ContinueSectionViewTestBase() override = default;

  // Whether we should run the test in tablet mode.
  bool tablet_mode_param() { return tablet_mode_; }

  ContinueSectionView* GetContinueSectionView() {
    if (Shell::Get()->tablet_mode_controller()->InTabletMode())
      return GetAppListTestHelper()->GetFullscreenContinueSectionView();
    return GetAppListTestHelper()->GetBubbleContinueSectionView();
  }

  views::View* GetRecentAppsView() {
    if (Shell::Get()->tablet_mode_controller()->InTabletMode())
      return GetAppListTestHelper()->GetFullscreenRecentAppsView();
    return GetAppListTestHelper()->GetBubbleRecentAppsView();
  }

  views::View* GetAppsGridView() {
    if (Shell::Get()->tablet_mode_controller()->InTabletMode())
      return GetAppListTestHelper()->GetRootPagedAppsGridView();
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  void AddSearchResult(const std::string& id, AppListSearchResultType type) {
    AddSearchResultToModel(
        id, type, AppListModelProvider::Get()->search_model(), "Fake Title");
  }

  void AddSearchResultWithTitle(const std::string& id,
                                AppListSearchResultType type,
                                const std::string& title) {
    AddSearchResultToModel(id, type,
                           AppListModelProvider::Get()->search_model(), title);
  }

  void RemoveSearchResultAt(size_t index) {
    GetAppListTestHelper()->GetSearchResults()->RemoveAt(index);
  }

  SearchModel::SearchResults* GetResults() {
    return GetAppListTestHelper()->GetSearchResults();
  }

  std::vector<SearchResult*> GetContinueResults() {
    auto continue_filter = [](const SearchResult& r) -> bool {
      return r.display_type() == SearchResultDisplayType::kContinue;
    };
    std::vector<SearchResult*> continue_results;
    continue_results = SearchModel::FilterSearchResultsByFunction(
        GetResults(), base::BindRepeating(continue_filter),
        /*max_results=*/4);

    return continue_results;
  }

  SearchBoxView* GetSearchBoxView() {
    if (Shell::Get()->tablet_mode_controller()->InTabletMode())
      return GetAppListTestHelper()->GetSearchBoxView();
    return GetAppListTestHelper()->GetBubbleSearchBoxView();
  }

  ContinueTaskView* GetResultViewAt(int index) {
    return GetContinueSectionView()->GetTaskViewAtForTesting(index);
  }

  std::vector<std::string> GetResultIds() {
    const size_t result_count =
        GetContinueSectionView()->GetTasksSuggestionsCount();
    std::vector<std::string> ids;
    for (size_t i = 0; i < result_count; ++i)
      ids.push_back(GetResultViewAt(i)->result()->id());
    return ids;
  }

  void VerifyResultViewsUpdated() {
    // Wait for the view to update any pending SearchResults.
    base::RunLoop().RunUntilIdle();

    std::vector<SearchResult*> results = GetContinueResults();
    for (size_t i = 0; i < results.size(); ++i)
      EXPECT_EQ(results[i], GetResultViewAt(i)->result()) << i;
  }

  void EnsureLauncherShown() {
    if (tablet_mode_param()) {
      // Convert to tablet mode to show fullscren launcher.
      Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
      test_api_ = std::make_unique<test::AppsGridViewTestApi>(
          GetAppListTestHelper()->GetRootPagedAppsGridView());
    } else {
      Shell::Get()->app_list_controller()->ShowAppList();
      test_api_ = std::make_unique<test::AppsGridViewTestApi>(
          GetAppListTestHelper()->GetScrollableAppsGridView());
    }
    ASSERT_TRUE(GetAppsGridView());
  }

  void SimulateRightClickOrLongPressAt(gfx::Point location) {
    ui::test::EventGenerator* generator = GetEventGenerator();
    if (tablet_mode_param()) {
      generator->set_current_screen_location(location);
      generator->PressTouch();
      ui::GestureEventDetails event_details(ui::ET_GESTURE_LONG_PRESS);
      ui::GestureEvent long_press(location.x(), location.y(), 0,
                                  base::TimeTicks::Now(), event_details);
      generator->Dispatch(&long_press);
      generator->ReleaseTouch();
      GetAppListTestHelper()->WaitUntilIdle();
    } else {
      generator->MoveMouseTo(location);
      generator->ClickRightButton();
    }
  }

  test::AppsGridViewTestApi* test_api() { return test_api_.get(); }

 private:
  bool tablet_mode_ = false;

  std::unique_ptr<test::AppsGridViewTestApi> test_api_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ContinueSectionViewTest : public ContinueSectionViewTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  ContinueSectionViewTest()
      : ContinueSectionViewTestBase(/*tablet_mode=*/GetParam()) {}
  ~ContinueSectionViewTest() override = default;
};

class ContinueSectionViewClamshellModeTest
    : public ContinueSectionViewTestBase {
 public:
  ContinueSectionViewClamshellModeTest()
      : ContinueSectionViewTestBase(/*tablet_mode=*/false) {}
  ~ContinueSectionViewClamshellModeTest() override = default;
};

class ContinueSectionViewTabletModeTest : public ContinueSectionViewTestBase {
 public:
  ContinueSectionViewTabletModeTest()
      : ContinueSectionViewTestBase(/*tablet_mode*/ true) {}
  ~ContinueSectionViewTabletModeTest() override = default;
};

// Instantiate the values in the parameterized tests. Used to toggle tablet
// mode.
INSTANTIATE_TEST_SUITE_P(All, ContinueSectionViewTest, testing::Bool());

TEST_P(ContinueSectionViewTest, CreatesViewsForTasks) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  ContinueSectionView* view = GetContinueSectionView();
  EXPECT_EQ(view->GetTasksSuggestionsCount(), 2u);
}

TEST_P(ContinueSectionViewTest, VerifyAddedViewsOrder) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 3u);
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  EXPECT_EQ(GetResultViewAt(1)->result()->id(), "id2");
  EXPECT_EQ(GetResultViewAt(2)->result()->id(), "id3");
}

TEST_P(ContinueSectionViewTest, ModelObservers) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  // Remove from end.
  GetResults()->DeleteAt(2);
  VerifyResultViewsUpdated();

  // Insert a new result.
  AddSearchResult("id4", AppListSearchResultType::kFileChip);
  VerifyResultViewsUpdated();

  // Delete from start.
  GetResults()->DeleteAt(0);
  VerifyResultViewsUpdated();
}

TEST_P(ContinueSectionViewTest, HideContinueSectionWhenResultRemoved) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  EXPECT_TRUE(GetContinueSectionView()->GetVisible());

  RemoveSearchResultAt(1);
  VerifyResultViewsUpdated();
  ASSERT_LE(GetContinueSectionView()->GetTasksSuggestionsCount(), 2u);

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ShowContinueSectionWhenResultAdded) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  EXPECT_FALSE(GetContinueSectionView()->GetVisible());

  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  VerifyResultViewsUpdated();

  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ClickOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);

  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  GetEventGenerator()->MoveMouseTo(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_P(ContinueSectionViewTest, TapOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);

  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  GetEventGenerator()->GestureTapAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_F(ContinueSectionViewClamshellModeTest, PressEnterOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  SearchBoxView* search_box_view = GetSearchBoxView();
  EXPECT_TRUE(search_box_view->search_box()->HasFocus());

  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(continue_task_view->HasFocus());

  PressAndReleaseKey(ui::VKEY_RETURN);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

// Regression test for https://crbug.com/1273170.
TEST_F(ContinueSectionViewClamshellModeTest, SearchAndCancelDoesNotChangeSize) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  EnsureLauncherShown();

  auto* apps_grid_view = GetAppListTestHelper()->GetScrollableAppsGridView();
  const gfx::Point apps_grid_origin = apps_grid_view->origin();

  auto* continue_section_view = GetContinueSectionView();
  const gfx::Rect continue_section_bounds = continue_section_view->bounds();
  auto* widget = continue_section_view->GetWidget();

  // Start a search.
  PressAndReleaseKey(ui::VKEY_A);

  // Simulate the suggestions changing.
  GetResults()->RemoveAll();
  AddSearchResult("id3", AppListSearchResultType::kFileChip);
  AddSearchResult("id4", AppListSearchResultType::kDriveChip);
  AddSearchResult("id5", AppListSearchResultType::kDriveChip);

  // Cancel the search.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  base::RunLoop().RunUntilIdle();  // Wait for SearchResult updates to views.
  widget->LayoutRootViewIfNecessary();

  // Continue section bounds did not grow.
  EXPECT_EQ(continue_section_bounds, continue_section_view->bounds());

  // Apps grid view position did not change.
  EXPECT_EQ(apps_grid_origin, apps_grid_view->origin());
}

TEST_P(ContinueSectionViewTest, RightClickOpensContextMenu) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(continue_task_view->IsMenuShowing());
}

TEST_P(ContinueSectionViewTest, OpenWithContextMenuOption) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(continue_task_view->IsMenuShowing());
  continue_task_view->ExecuteCommand(ContinueTaskCommandId::kOpenResult,
                                     ui::EventFlags::EF_NONE);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_P(ContinueSectionViewTest, RemoveWithContextMenuOption) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(continue_task_view->IsMenuShowing());
  continue_task_view->ExecuteCommand(ContinueTaskCommandId::kRemoveResult,
                                     ui::EventFlags::EF_NONE);

  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {"id1", SearchResultActionType::kRemove}};
  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      client->GetAndClearInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(ContinueSectionViewTest, ResultRemovedContextMenuCloses) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  AddSearchResult("id4", AppListSearchResultType::kFileChip);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(1);
  EXPECT_EQ(continue_task_view->result()->id(), "id2");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  RemoveSearchResultAt(1);
  VerifyResultViewsUpdated();

  ASSERT_EQ(std::vector<std::string>({"id1", "id3", "id4"}), GetResultIds());

  // Click on another result and verify it activates the item to confirm the
  // event is not consumed by a context menu.
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  GetEventGenerator()->MoveMouseTo(
      GetResultViewAt(0)->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_P(ContinueSectionViewTest, UpdateAppsOnModelChange) {
  AddSearchResult("id11", AppListSearchResultType::kFileChip);
  AddSearchResult("id12", AppListSearchResultType::kDriveChip);
  AddSearchResult("id13", AppListSearchResultType::kDriveChip);
  AddSearchResult("id14", AppListSearchResultType::kFileChip);
  UpdateDisplay("1200x800");
  EnsureLauncherShown();

  EXPECT_EQ(std::vector<std::string>({"id11", "id12", "id13", "id14"}),
            GetResultIds());

  // Update active model, and make sure the shown results view get updated
  // accordingly.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();

  AddSearchResultToModel("id21", AppListSearchResultType::kFileChip,
                         search_model_override.get(), "Fake Title");
  AddSearchResultToModel("id22", AppListSearchResultType::kFileChip,
                         search_model_override.get(), "Fake Title");
  AddSearchResultToModel("id23", AppListSearchResultType::kFileChip,
                         search_model_override.get(), "Fake Title");

  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get());
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(std::vector<std::string>({"id21", "id22", "id23"}), GetResultIds());

  // Tap a result, and verify it gets activated.
  GetEventGenerator()->GestureTapAt(
      GetResultViewAt(2)->GetBoundsInScreen().CenterPoint());

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id23", client->last_opened_search_result());

  // Results should be cleared if app list models get reset.
  Shell::Get()->app_list_controller()->ClearActiveModel();
  EXPECT_EQ(std::vector<std::string>{}, GetResultIds());
}

TEST_F(ContinueSectionViewTabletModeTest,
       TabletModeLayoutWithThreeSuggestions) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  UpdateDisplay("1200x800");
  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ASSERT_TRUE(GetContinueSectionView()->GetVisible());
  ASSERT_EQ(3u, GetContinueSectionView()->GetTasksSuggestionsCount());

  const gfx::Size size = GetResultViewAt(0)->size();
  const int vertical_position = GetResultViewAt(0)->y();

  for (int i = 1; i < 3; i++) {
    const ContinueTaskView* task_view = GetResultViewAt(i);
    EXPECT_TRUE(task_view->GetVisible());
    EXPECT_EQ(size, task_view->size());
    EXPECT_EQ(vertical_position, task_view->y());
    EXPECT_GT(task_view->x(), GetResultViewAt(i - 1)->bounds().right());
  }

  views::View* parent_view = GetResultViewAt(0)->parent();

  EXPECT_EQ(std::abs(parent_view->GetContentsBounds().x() -
                     GetResultViewAt(0)->bounds().x()),
            std::abs(parent_view->GetContentsBounds().right() -
                     GetResultViewAt(2)->bounds().right()));
}

TEST_F(ContinueSectionViewTabletModeTest, TabletModeLayoutWithFourSuggestions) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  AddSearchResult("id4", AppListSearchResultType::kFileChip);

  UpdateDisplay("1200x800");
  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_TRUE(GetContinueSectionView()->GetVisible());
  ASSERT_EQ(4u, GetContinueSectionView()->GetTasksSuggestionsCount());

  const gfx::Size size = GetResultViewAt(0)->size();
  const int vertical_position = GetResultViewAt(0)->y();

  for (int i = 1; i < 4; i++) {
    const ContinueTaskView* task_view = GetResultViewAt(i);
    EXPECT_TRUE(task_view->GetVisible());
    EXPECT_EQ(size, task_view->size());
    EXPECT_EQ(vertical_position, task_view->y());
    EXPECT_GT(task_view->x(), GetResultViewAt(i - 1)->bounds().right());
  }

  views::View* parent_view = GetResultViewAt(0)->parent();

  EXPECT_EQ(std::abs(parent_view->GetContentsBounds().x() -
                     GetResultViewAt(0)->bounds().x()),
            std::abs(parent_view->GetContentsBounds().right() -
                     GetResultViewAt(3)->bounds().right()));
}

TEST_P(ContinueSectionViewTest, NoOverlapsWithRecentApps) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  AddSearchResult("id4", AppListSearchResultType::kFileChip);
  GetAppListTestHelper()->AddRecentApps(5);
  GetAppListTestHelper()->AddAppItems(5);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_FALSE(GetContinueSectionView()->bounds().Intersects(
      GetRecentAppsView()->bounds()));
}

TEST_P(ContinueSectionViewTest, NoOverlapsWithAppsGridItems) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  AddSearchResult("id4", AppListSearchResultType::kFileChip);
  GetAppListTestHelper()->AddAppItems(5);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  gfx::Rect continue_bounds = GetContinueSectionView()->GetBoundsInScreen();
  for (size_t i = 0; i < test_api()->GetItemList()->item_count(); i++) {
    gfx::Rect app_bounds =
        test_api()->GetViewAtModelIndex(i)->GetBoundsInScreen();
    EXPECT_FALSE(continue_bounds.Intersects(app_bounds)) << i;
  }
}

// Tests that number of shown continue section items is reduced if they don't
// all fit into the available space (while maintaining min width).
TEST_F(ContinueSectionViewTabletModeTest,
       TabletModeLayoutWithFourSuggestionsWithRestrictedSpace) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);
  AddSearchResult("id4", AppListSearchResultType::kFileChip);

  // Set the display width so only 2 continue section tasks fit into available
  // space.
  UpdateDisplay("600x800");

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ASSERT_EQ(4u, GetContinueSectionView()->GetTasksSuggestionsCount());

  // Only first two tasks are visible.
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(i <= 1, GetResultViewAt(i)->GetVisible()) << i;

  const ContinueTaskView* first_task = GetResultViewAt(0);
  const ContinueTaskView* second_task = GetResultViewAt(1);

  EXPECT_EQ(first_task->size(), second_task->size());
  EXPECT_EQ(first_task->y(), second_task->y());
  EXPECT_GT(second_task->x(), first_task->bounds().right());
}

TEST_P(ContinueSectionViewTest, AllTasksShareTheSameWidth) {
  AddSearchResultWithTitle("id1", AppListSearchResultType::kFileChip, "title");
  AddSearchResultWithTitle(
      "id2", AppListSearchResultType::kDriveChip,
      "Really really really long title text for the label");
  AddSearchResultWithTitle("id3", AppListSearchResultType::kDriveChip, "-");
  AddSearchResultWithTitle("id4", AppListSearchResultType::kFileChip,
                           "medium title");

  UpdateDisplay("1200x800");
  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ASSERT_EQ(4u, GetContinueSectionView()->GetTasksSuggestionsCount());

  const gfx::Size size = GetResultViewAt(0)->size();

  for (int i = 1; i < 4; i++) {
    const ContinueTaskView* task_view = GetResultViewAt(i);
    EXPECT_TRUE(task_view->GetVisible());
    EXPECT_EQ(size, task_view->size());
  }
}

TEST_P(ContinueSectionViewTest, HideContinueSectionWhithLessThanMinimumFiles) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  // Wait for the view to update any pending SearchResults.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ShowContinueSectionWhithMinimumFiles) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();

  // Wait for the view to update any pending SearchResults.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, TaskViewHasRippleWithMenuOpen) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            views::InkDrop::Get(continue_task_view)
                ->GetInkDrop()
                ->GetTargetInkDropState());
}

TEST_P(ContinueSectionViewTest, TaskViewHidesRippleAfterMenuCloses) {
  AddSearchResult("id1", AppListSearchResultType::kFileChip);
  AddSearchResult("id2", AppListSearchResultType::kDriveChip);
  AddSearchResult("id3", AppListSearchResultType::kDriveChip);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      continue_task_view->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  // Click on other task view to hide context menu.
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressAt(
      GetResultViewAt(2)->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(continue_task_view->IsMenuShowing());

  // Wait for the view to update the ink drop.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(views::InkDropState::HIDDEN, views::InkDrop::Get(continue_task_view)
                                             ->GetInkDrop()
                                             ->GetTargetInkDropState());
}

}  // namespace
}  // namespace ash
