// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_test_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/test_search_result.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_toast_container_view.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/app_list/views/recent_apps_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

using test::AppListTestViewDelegate;

std::unique_ptr<TestSearchResult> CreateTestResult(const std::string& id,
                                                   AppListSearchResultType type,
                                                   const std::string& title) {
  auto result = std::make_unique<TestSearchResult>();
  result->set_result_id(id);
  result->SetTitle(base::ASCIIToUTF16(title));
  result->set_result_type(type);
  result->set_display_type(SearchResultDisplayType::kContinue);
  return result;
}

void AddSearchResultToModel(const std::string& id,
                            AppListSearchResultType type,
                            SearchModel* model,
                            const std::string& title) {
  model->results()->Add(CreateTestResult(id, type, title));
}

void WaitForAllChildrenAnimationsToComplete(views::View* view) {
  while (true) {
    bool found_animation = false;
    for (views::View* child : view->children()) {
      if (child->layer() && child->layer()->GetAnimator()->is_animating()) {
        ui::LayerAnimationStoppedWaiter waiter;
        waiter.Wait(child->layer());
        found_animation = true;
        break;
      }
    }

    if (!found_animation)
      return;
  }
}

class ContinueSectionViewTestBase : public AshTestBase {
 public:
  explicit ContinueSectionViewTestBase(bool tablet_mode)
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        tablet_mode_(tablet_mode) {}
  ~ContinueSectionViewTestBase() override = default;

  void TearDown() override {
    AshTestBase::TearDown();
    // Clean up global variables used for metrics on continue section removals.
    ContinueSectionView::ResetContinueSectionFileRemovalMetricEnabledForTest();
    ResetContinueSectionFileRemovedCountForTest();
  }

  // Whether we should run the test in tablet mode.
  bool tablet_mode_param() { return tablet_mode_; }

  // Sets up the continue section view (and app list) state for testing continue
  // section update animations. `result_count` is the number of continue tasks
  // that should initially be added to search model.
  // Returns the list of current task view layer bounds.
  std::vector<gfx::RectF> InitializeForAnimationTest(int result_count) {
    // Ensure display is large enough to display 4 chips in tablet mode.
    UpdateDisplay("1200x800");

    AddTestSearchResults(result_count);

    EnsureLauncherShown();
    base::RunLoop().RunUntilIdle();
    base::OneShotTimer* animations_timer = GetContinueSectionView()
                                               ->suggestions_container()
                                               ->animations_timer_for_test();
    if (animations_timer)
      animations_timer->FireNow();

    animation_duration_.emplace(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    return GetCurrentLayerBoundsForAllTaskViews();
  }

  ContinueSectionView* GetContinueSectionView() {
    if (display::Screen::GetScreen()->InTabletMode()) {
      return GetAppListTestHelper()->GetFullscreenContinueSectionView();
    }
    return GetAppListTestHelper()->GetBubbleContinueSectionView();
  }

  AppListNudgeController* GetAppListNudgeController() {
    return GetContinueSectionView()->nudge_controller_for_test();
  }

  views::View* GetRecentAppsView() {
    if (display::Screen::GetScreen()->InTabletMode()) {
      return GetAppListTestHelper()->GetFullscreenRecentAppsView();
    }
    return GetAppListTestHelper()->GetBubbleRecentAppsView();
  }

  views::View* GetAppsGridView() {
    if (display::Screen::GetScreen()->InTabletMode()) {
      return GetAppListTestHelper()->GetRootPagedAppsGridView();
    }
    return GetAppListTestHelper()->GetScrollableAppsGridView();
  }

  void AddTestSearchResults(int count) {
    for (int i = 0; i < count; ++i)
      AddSearchResult(base::StringPrintf("id_%d", i),
                      AppListSearchResultType::kZeroStateFile);
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
    if (display::Screen::GetScreen()->InTabletMode()) {
      return GetAppListTestHelper()->GetSearchBoxView();
    }
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
      ash::TabletModeControllerTestApi().EnterTabletMode();
      Shell::Get()->app_list_controller()->ShowAppList(
          AppListShowSource::kSearchKey);
      test_api_ = std::make_unique<test::AppsGridViewTestApi>(
          GetAppListTestHelper()->GetRootPagedAppsGridView());
    } else {
      Shell::Get()->app_list_controller()->ShowAppList(
          AppListShowSource::kSearchKey);
      test_api_ = std::make_unique<test::AppsGridViewTestApi>(
          GetAppListTestHelper()->GetScrollableAppsGridView());
    }
    ASSERT_TRUE(GetAppsGridView());
  }

  void HideLauncher() { Shell::Get()->app_list_controller()->DismissAppList(); }

  void ResetPrivacyNoticePref() {
    AppListNudgeController::SetPrivacyNoticeAcceptedForTest(false);
  }

  void SimulateRightClickOrLongPressOn(const views::View* view) {
    gfx::Point location = view->GetBoundsInScreen().CenterPoint();
    if (tablet_mode_param()) {
      ui::test::EventGenerator* generator = GetEventGenerator();
      generator->set_current_screen_location(location);
      generator->PressTouch();
      ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
      ui::GestureEvent long_press(location.x(), location.y(), 0,
                                  base::TimeTicks::Now(), event_details);
      generator->Dispatch(&long_press);
      generator->ReleaseTouch();
      GetAppListTestHelper()->WaitUntilIdle();
    } else {
      RightClickOn(view);
    }
  }

  bool IsPrivacyNoticeVisible() {
    auto* privacy_notice = GetContinueSectionView()->GetPrivacyNoticeForTest();
    return privacy_notice && privacy_notice->GetVisible();
  }

  gfx::RectF GetTargetLayerBounds(views::View* view) {
    return view->layer()->GetTargetTransform().MapRect(
        gfx::RectF(view->layer()->GetTargetBounds()));
  }

  gfx::RectF GetCurrentLayerBounds(views::View* view) {
    return view->layer()->transform().MapRect(
        gfx::RectF(view->layer()->bounds()));
  }

  std::vector<gfx::RectF> GetCurrentLayerBoundsForAllTaskViews() {
    std::vector<gfx::RectF> bounds;
    const size_t count = GetContinueSectionView()->GetTasksSuggestionsCount();
    bounds.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      views::View* result_view = GetResultViewAt(i);
      bounds.push_back(GetCurrentLayerBounds(result_view));
    }
    return bounds;
  }

  void RemoveSearchResultWithContextMenuAt(int index) {
    ContinueTaskView* continue_task_view = GetResultViewAt(index);

    GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
    SimulateRightClickOrLongPressOn(continue_task_view);
    EXPECT_TRUE(continue_task_view->IsMenuShowing()) << index;
    continue_task_view->ExecuteCommand(ContinueTaskCommandId::kRemoveResult,
                                       ui::EF_NONE);
    EXPECT_FALSE(continue_task_view->IsMenuShowing());
  }

  test::AppsGridViewTestApi* test_api() { return test_api_.get(); }

 private:
  bool tablet_mode_ = false;

  std::optional<ui::ScopedAnimationDurationScaleMode> animation_duration_;
  std::unique_ptr<test::AppsGridViewTestApi> test_api_;
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
      : ContinueSectionViewTestBase(/*tablet_mode=*/true) {}
  ~ContinueSectionViewTabletModeTest() override = default;
};

class ContinueSectionViewWithReorderNudgeTest
    : public ContinueSectionViewTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ContinueSectionViewWithReorderNudgeTest()
      : ContinueSectionViewTestBase(/*tablet_mode=*/GetParam()) {}
  ~ContinueSectionViewWithReorderNudgeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    GetAppListTestHelper()->DisableAppListNudge(false);
  }

  AppListToastContainerView* GetToastContainerView() {
    if (!display::Screen::GetScreen()->InTabletMode()) {
      return GetAppListTestHelper()
          ->GetBubbleAppsPage()
          ->toast_container_for_test();
    }

    return GetAppListTestHelper()->GetAppsContainerView()->toast_container();
  }
};

// Instantiate the values in the parameterized tests. Used to toggle tablet
// mode.
INSTANTIATE_TEST_SUITE_P(All, ContinueSectionViewTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         ContinueSectionViewWithReorderNudgeTest,
                         testing::Bool());

TEST_P(ContinueSectionViewTest, CreatesViewsForTasks) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  ContinueSectionView* view = GetContinueSectionView();
  EXPECT_EQ(view->GetTasksSuggestionsCount(), 2u);
}

TEST_P(ContinueSectionViewTest, VerifyAddedViewsOrder) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 3u);
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  EXPECT_EQ(GetResultViewAt(1)->result()->id(), "id2");
  EXPECT_EQ(GetResultViewAt(2)->result()->id(), "id3");
}

// Tests that the continue section view will be visible once we have a admin
// template.
TEST_P(ContinueSectionViewTest, ShowContinueSectionWhenAdminTemplateAvailable) {
  base::test::ScopedFeatureList scoped_list;

  AddSearchResult("id", AppListSearchResultType::kDesksAdminTemplate);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 1u);
  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ShowsHelpAppResults) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateHelpApp);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  EXPECT_TRUE(GetContinueSectionView()->GetVisible());

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 4u);
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  EXPECT_EQ(GetResultViewAt(1)->result()->id(), "id2");
  EXPECT_EQ(GetResultViewAt(2)->result()->id(), "id3");
  EXPECT_EQ(GetResultViewAt(3)->result()->id(), "id4");
}

TEST_P(ContinueSectionViewTest, HelpAppResultNotShownWithoutEnoughOtherFiles) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateHelpApp);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 2u);
  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, HelpAppShownInTabletModeWith2FileResults) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateHelpApp);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateFile);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueSectionView* view = GetContinueSectionView();
  ASSERT_EQ(view->GetTasksSuggestionsCount(), 3u);
  EXPECT_EQ(GetParam(), GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ModelObservers) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  // Remove from end.
  GetResults()->DeleteAt(2);
  VerifyResultViewsUpdated();

  // Insert a new result.
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);
  VerifyResultViewsUpdated();

  // Delete from start.
  GetResults()->DeleteAt(0);
  VerifyResultViewsUpdated();
}

TEST_P(ContinueSectionViewTest, HideContinueSectionWhenResultRemoved) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  EXPECT_TRUE(GetContinueSectionView()->GetVisible());

  RemoveSearchResultAt(1);
  VerifyResultViewsUpdated();
  ASSERT_LE(GetContinueSectionView()->GetTasksSuggestionsCount(), 2u);

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ShowContinueSectionWhenResultAdded) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  EXPECT_FALSE(GetContinueSectionView()->GetVisible());

  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  VerifyResultViewsUpdated();

  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ClickOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);

  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  LeftClickOn(continue_task_view);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_P(ContinueSectionViewTest, TapOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);

  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  GestureTapOn(continue_task_view);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_F(ContinueSectionViewClamshellModeTest, PressEnterOpensSearchResult) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

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

TEST_F(ContinueSectionViewClamshellModeTest, DownArrowMovesFocusVertically) {
  AddSearchResult("id0", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueTaskView* task0 = GetResultViewAt(0);
  ContinueTaskView* task1 = GetResultViewAt(1);
  ContinueTaskView* task2 = GetResultViewAt(2);
  ContinueTaskView* task3 = GetResultViewAt(3);
  ASSERT_FALSE(task0->HasFocus());
  ASSERT_FALSE(task1->HasFocus());
  ASSERT_FALSE(task2->HasFocus());
  ASSERT_FALSE(task3->HasFocus());

  // Down arrow from search box moves to first continue task.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_TRUE(task0->HasFocus());
  EXPECT_FALSE(task1->HasFocus());
  EXPECT_FALSE(task2->HasFocus());
  EXPECT_FALSE(task3->HasFocus());

  // Down arrow from first continue task moves down by one row, focusing
  // `task2` instead of the next task in tab order (`task1`).
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(task0->HasFocus());
  EXPECT_FALSE(task1->HasFocus());
  EXPECT_TRUE(task2->HasFocus());
  EXPECT_FALSE(task3->HasFocus());

  // Down again moves focus out of continue section.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_FALSE(task0->HasFocus());
  EXPECT_FALSE(task1->HasFocus());
  EXPECT_FALSE(task2->HasFocus());
  EXPECT_FALSE(task3->HasFocus());
}

TEST_F(ContinueSectionViewClamshellModeTest, UpArrowMovesFocusVertically) {
  AddSearchResult("id0", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueTaskView* task0 = GetResultViewAt(0);
  ContinueTaskView* task1 = GetResultViewAt(1);
  ContinueTaskView* task2 = GetResultViewAt(2);
  ContinueTaskView* task3 = GetResultViewAt(3);
  ASSERT_FALSE(task0->HasFocus());
  ASSERT_FALSE(task1->HasFocus());
  ASSERT_FALSE(task2->HasFocus());
  ASSERT_FALSE(task3->HasFocus());

  // Focus the last task (in the second row, second column).
  task3->RequestFocus();
  EXPECT_FALSE(task0->HasFocus());
  EXPECT_FALSE(task1->HasFocus());
  EXPECT_FALSE(task2->HasFocus());
  EXPECT_TRUE(task3->HasFocus());

  // Up arrow moves up by one row, focusing `task1` instead of the previous
  // task in tab order (`task2`).
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_FALSE(task0->HasFocus());
  EXPECT_TRUE(task1->HasFocus());
  EXPECT_FALSE(task2->HasFocus());
  EXPECT_FALSE(task3->HasFocus());

  // Up again moves focus out of continue section.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_FALSE(task0->HasFocus());
  EXPECT_FALSE(task1->HasFocus());
  EXPECT_FALSE(task2->HasFocus());
  EXPECT_FALSE(task3->HasFocus());
}

TEST_F(ContinueSectionViewClamshellModeTest, FocusingChipScrollsToShowLabel) {
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  // Add enough apps to allow the apps page to scroll.
  helper->AddAppItems(50);
  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  // Up arrow moves focus to the last row of the apps grid, forcing the apps
  // page to scroll.
  PressAndReleaseKey(ui::VKEY_UP);
  auto* scroll_view = helper->GetBubbleAppsPage()->scroll_view();
  const int initial_scroll_offset = scroll_view->GetVisibleRect().y();
  ASSERT_GT(initial_scroll_offset, 0);

  // Down arrow twice moves focus to search box, then to continue section.
  PressAndReleaseKey(ui::VKEY_DOWN);
  PressAndReleaseKey(ui::VKEY_DOWN);
  ASSERT_TRUE(GetResultViewAt(0)->HasFocus());

  // Scroll view has scrolled to the top to reveal the label.
  const int final_scroll_offset = scroll_view->GetVisibleRect().y();
  EXPECT_EQ(final_scroll_offset, 0);
}

TEST_F(ContinueSectionViewClamshellModeTest,
       FocusingPrivacyNoticeScrollsToShowNotice) {
  auto* helper = GetAppListTestHelper();
  helper->AddContinueSuggestionResults(4);
  // Add enough apps to allow the apps page to scroll.
  helper->AddAppItems(50);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  ASSERT_TRUE(IsPrivacyNoticeVisible());

  // Up arrow moves focus to the last row of the apps grid, forcing the apps
  // page to scroll.
  PressAndReleaseKey(ui::VKEY_UP);
  auto* scroll_view = helper->GetBubbleAppsPage()->scroll_view();
  const int initial_scroll_offset = scroll_view->GetVisibleRect().y();
  ASSERT_GT(initial_scroll_offset, 0);

  // Down arrow twice moves focus to search box, then to privacy notice.
  PressAndReleaseKey(ui::VKEY_DOWN);
  PressAndReleaseKey(ui::VKEY_DOWN);
  auto* privacy_notice = GetContinueSectionView()->GetPrivacyNoticeForTest();
  ASSERT_TRUE(privacy_notice->toast_button()->HasFocus());

  // Scroll view has scrolled to the top to reveal the whole notice.
  const int final_scroll_offset = scroll_view->GetVisibleRect().y();
  EXPECT_EQ(final_scroll_offset, 0);
}

// Regression test for https://crbug.com/1273170.
TEST_F(ContinueSectionViewClamshellModeTest, SearchAndCancelDoesNotChangeSize) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
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
  AddSearchResult("id3", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id5", AppListSearchResultType::kZeroStateDrive);

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
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressOn(continue_task_view);
  EXPECT_TRUE(continue_task_view->IsMenuShowing());
}

TEST_P(ContinueSectionViewTest, OpenWithContextMenuOption) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressOn(continue_task_view);
  EXPECT_TRUE(continue_task_view->IsMenuShowing());
  continue_task_view->ExecuteCommand(ContinueTaskCommandId::kOpenResult,
                                     ui::EF_NONE);

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_P(ContinueSectionViewTest, RemoveWithContextMenuOption) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  RemoveSearchResultWithContextMenuAt(0);

  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {"id1", SearchResultActionType::kRemove}};
  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      client->GetAndResetInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);
}

TEST_P(ContinueSectionViewTest, ResultRemovedLogsMetricInBucket) {
  base::HistogramTester histogram_tester;
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  // Remove two results.
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  RemoveSearchResultWithContextMenuAt(0);

  EXPECT_EQ(GetResultViewAt(1)->result()->id(), "id2");
  RemoveSearchResultWithContextMenuAt(1);

  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  std::vector<TestAppListClient::SearchResultActionId> expected_actions = {
      {"id1", SearchResultActionType::kRemove},
      {"id2", SearchResultActionType::kRemove}};
  std::vector<TestAppListClient::SearchResultActionId> invoked_actions =
      client->GetAndResetInvokedResultActions();
  EXPECT_EQ(expected_actions, invoked_actions);

  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   kContinueSectionFilesRemovedInSessionHistogram, 1));
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   kContinueSectionFilesRemovedInSessionHistogram, 2));
  histogram_tester.ExpectTotalCount(
      kContinueSectionFilesRemovedInSessionHistogram, 2);
}

TEST_P(ContinueSectionViewTest, ResultRemovedContextMenuCloses) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(1);
  EXPECT_EQ(continue_task_view->result()->id(), "id2");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressOn(continue_task_view);
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  RemoveSearchResultAt(1);
  VerifyResultViewsUpdated();

  ASSERT_EQ(std::vector<std::string>({"id1", "id3", "id4"}), GetResultIds());

  // Click on another result and verify it activates the item to confirm the
  // event is not consumed by a context menu.
  EXPECT_EQ(GetResultViewAt(0)->result()->id(), "id1");
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  LeftClickOn(GetResultViewAt(0));

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id1", client->last_opened_search_result());
}

TEST_P(ContinueSectionViewTest, UpdateAppsOnModelChange) {
  AddSearchResult("id11", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id12", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id13", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id14", AppListSearchResultType::kZeroStateFile);
  UpdateDisplay("1200x800");
  EnsureLauncherShown();

  EXPECT_EQ(std::vector<std::string>({"id11", "id12", "id13", "id14"}),
            GetResultIds());

  // Update active model, and make sure the shown results view get updated
  // accordingly.
  auto model_override = std::make_unique<test::AppListTestModel>();
  auto search_model_override = std::make_unique<SearchModel>();
  auto quick_app_access_model_override =
      std::make_unique<QuickAppAccessModel>();

  AddSearchResultToModel("id21", AppListSearchResultType::kZeroStateFile,
                         search_model_override.get(), "Fake Title");
  AddSearchResultToModel("id22", AppListSearchResultType::kZeroStateFile,
                         search_model_override.get(), "Fake Title");
  AddSearchResultToModel("id23", AppListSearchResultType::kZeroStateFile,
                         search_model_override.get(), "Fake Title");

  Shell::Get()->app_list_controller()->SetActiveModel(
      /*profile_id=*/1, model_override.get(), search_model_override.get(),
      quick_app_access_model_override.get());
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(std::vector<std::string>({"id21", "id22", "id23"}), GetResultIds());

  // Tap a result, and verify it gets activated.
  GestureTapOn(GetResultViewAt(2));

  // The item was activated.
  TestAppListClient* client = GetAppListTestHelper()->app_list_client();
  EXPECT_EQ("id23", client->last_opened_search_result());

  // Results should be cleared if app list models get reset.
  Shell::Get()->app_list_controller()->ClearActiveModel();
  EXPECT_EQ(std::vector<std::string>{}, GetResultIds());
}

TEST_F(ContinueSectionViewTabletModeTest,
       TabletModeLayoutWithThreeSuggestions) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

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
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);

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
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);
  GetAppListTestHelper()->AddRecentApps(5);
  GetAppListTestHelper()->AddAppItems(5);

  EnsureLauncherShown();

  VerifyResultViewsUpdated();
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_FALSE(GetContinueSectionView()->bounds().Intersects(
      GetRecentAppsView()->bounds()));
}

TEST_P(ContinueSectionViewTest, NoOverlapsWithAppsGridItems) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);
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
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id4", AppListSearchResultType::kZeroStateFile);

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
  AddSearchResultWithTitle("id1", AppListSearchResultType::kZeroStateFile,
                           "title");
  AddSearchResultWithTitle(
      "id2", AppListSearchResultType::kZeroStateDrive,
      "Really really really long title text for the label");
  AddSearchResultWithTitle("id3", AppListSearchResultType::kZeroStateDrive,
                           "-");
  AddSearchResultWithTitle("id4", AppListSearchResultType::kZeroStateFile,
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
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  // Wait for the view to update any pending SearchResults.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, ShowContinueSectionWhithMinimumFiles) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);

  // Minimum files for clamshell mode are 3.
  if (!tablet_mode_param())
    AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();

  // Wait for the view to update any pending SearchResults.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, TaskViewHasRippleWithMenuOpen) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressOn(continue_task_view);
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            views::InkDrop::Get(continue_task_view)
                ->GetInkDrop()
                ->GetTargetInkDropState());
}

TEST_P(ContinueSectionViewTest, TaskViewHidesRippleAfterMenuCloses) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ContinueTaskView* continue_task_view = GetResultViewAt(0);
  EXPECT_EQ(continue_task_view->result()->id(), "id1");

  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressOn(continue_task_view);
  EXPECT_TRUE(continue_task_view->IsMenuShowing());

  // Click on other task view to hide context menu.
  GetContinueSectionView()->GetWidget()->LayoutRootViewIfNecessary();
  SimulateRightClickOrLongPressOn(GetResultViewAt(2));
  EXPECT_FALSE(continue_task_view->IsMenuShowing());

  // Wait for the view to update the ink drop.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(views::InkDropState::HIDDEN, views::InkDrop::Get(continue_task_view)
                                             ->GetInkDrop()
                                             ->GetTargetInkDropState());
}

TEST_P(ContinueSectionViewWithReorderNudgeTest, ShowPrivacyNotice) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
}

TEST_P(ContinueSectionViewWithReorderNudgeTest, AcceptPrivacyNotice) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);

  GestureTapOn(
      GetContinueSectionView()->GetPrivacyNoticeForTest()->toast_button());

  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());

  HideLauncher();
  EnsureLauncherShown();

  EXPECT_FALSE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kReorderNudge);
  EXPECT_EQ(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
}

TEST_P(ContinueSectionViewWithReorderNudgeTest, TimeDismissPrivacyNotice) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);

  ASSERT_TRUE(GetContinueSectionView()->FirePrivacyNoticeShownTimerForTest());

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
}

TEST_P(ContinueSectionViewWithReorderNudgeTest,
       HidingContinueSectionHidesPrivacyNotice) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  ASSERT_TRUE(IsPrivacyNoticeVisible());
  ASSERT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  // Simulate the user hiding the continue section.
  Shell::Get()->app_list_controller()->SetHideContinueSection(true);
  EXPECT_FALSE(GetContinueSectionView()->GetVisible());

  // The privacy notice is suppressed.
  EXPECT_FALSE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kNone);
}

TEST_P(ContinueSectionViewWithReorderNudgeTest,
       DoNotShowPrivacyNoticeAndReorderNudgeAlternitively) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);

  // Close the launcher without waiting long enough for the privacy notice to be
  // considered shown.
  HideLauncher();

  // The privacy notice should be showing instead of the reorder nudge when the
  // next time the launcher is open.
  EnsureLauncherShown();
  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
}

TEST_P(ContinueSectionViewWithReorderNudgeTest,
       DoNotShowPrivacyNoticeAndReorderNudgeAlternitively2) {
  // Open the launcher without any search result added.
  EnsureLauncherShown();

  // Reorder nudge should be showing and privacy notice should not.
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());
  EXPECT_FALSE(GetContinueSectionView()->GetPrivacyNoticeForTest());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kReorderNudge);
  EXPECT_EQ(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);

  // Wait for long enough for the reorder nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));
  HideLauncher();

  // After hiding the launcher, add some search results and open the launcher
  // again.
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  // If the reorder nudge was shown and hasn't reached the shown times limit,
  // keep showing the reorder nudge instead of the privacy notice.
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());
  EXPECT_FALSE(GetContinueSectionView()->GetPrivacyNoticeForTest());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kReorderNudge);
  EXPECT_EQ(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
  // Wait for long enough for the reorder nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));
  HideLauncher();

  EnsureLauncherShown();
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());
  EXPECT_FALSE(GetContinueSectionView()->GetPrivacyNoticeForTest());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kReorderNudge);
  EXPECT_EQ(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
  // Wait for long enough for the reorder nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));
  HideLauncher();

  EnsureLauncherShown();
  // After the reorder nudge has been shown for three times, starts showing the
  // privacy notice and removes the reorder nudge.
  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
}

TEST_P(ContinueSectionViewWithReorderNudgeTest,
       SearchResultsFetchedAfterLauncherShown) {
  ResetPrivacyNoticePref();
  // Open the launcher without any search result added.
  EnsureLauncherShown();

  // Reorder nudge should be showing and privacy notice should not.
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());
  EXPECT_FALSE(GetContinueSectionView()->GetPrivacyNoticeForTest());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kReorderNudge);
  EXPECT_EQ(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);

  // Add some search results while the launcher is open.
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  VerifyResultViewsUpdated();

  // Neither the privacy notice nor the search results should show.
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowFilesSection());
  EXPECT_FALSE(GetContinueSectionView()->GetPrivacyNoticeForTest());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kReorderNudge);
  EXPECT_EQ(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);

  // Wait for long enough for the reorder nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));
  HideLauncher();

  // Open the launcher, wait for the reorder nudge to be shown and close it two
  // more times. After the reorder nudge has been shown for three times, starts
  // showing the privacy notice and removes the reorder nudge.

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  // Wait for long enough for the reorder nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));
  HideLauncher();
  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  // Wait for long enough for the reorder nudge to be considered shown.
  task_environment()->AdvanceClock(base::Seconds(1));
  HideLauncher();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();
  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);
  EXPECT_NE(GetToastContainerView()->current_toast(),
            AppListToastType::kReorderNudge);
  HideLauncher();
}

TEST_F(ContinueSectionViewTabletModeTest, PrivacyNoticeIsShownInBackground) {
  AddSearchResult("id1", AppListSearchResultType::kZeroStateFile);
  AddSearchResult("id2", AppListSearchResultType::kZeroStateDrive);
  AddSearchResult("id3", AppListSearchResultType::kZeroStateDrive);
  ResetPrivacyNoticePref();

  EnsureLauncherShown();
  VerifyResultViewsUpdated();

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  // Activate the search box. The privacy notice will become inactive but the
  // view still exists.
  auto* search_box = GetAppListTestHelper()->GetSearchBoxView();
  search_box->SetSearchBoxActive(true, ui::EventType::kMousePressed);

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  // The privacy notice is not considered as shown when the search box is
  // active. Therefore the privacy notice timer should not be running.
  EXPECT_FALSE(GetContinueSectionView()->FirePrivacyNoticeShownTimerForTest());

  // Reopen the app list.
  HideLauncher();
  EnsureLauncherShown();

  // As the privacy notice was not active during the last shown, shows the
  // notice again.
  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  // Activate the search box and then deactivate it.
  search_box->SetSearchBoxActive(true, ui::EventType::kMousePressed);
  search_box->SetSearchBoxActive(false, ui::EventType::kMousePressed);

  // The privacy notice timer should be restarted after the search box becomes
  // inactive.
  EXPECT_TRUE(GetContinueSectionView()->FirePrivacyNoticeShownTimerForTest());

  // Reopen the app list. The privacy notice should be removed.
  HideLauncher();
  EnsureLauncherShown();
  EXPECT_FALSE(GetContinueSectionView()->ShouldShowPrivacyNotice());
  EXPECT_FALSE(GetContinueSectionView()->GetPrivacyNoticeForTest());
}

TEST_P(ContinueSectionViewTest, InitialShowDoesNotAnimate) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/5);
  ASSERT_EQ(4u, initial_bounds.size());

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      GetContinueSectionView()->suggestions_container()->children();
  ASSERT_EQ(4u, container_children.size());
  for (int i = 0; i < 4; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->layer()->GetAnimator()->is_animating());
  }
}

TEST_P(ContinueSectionViewTest, UpdateWithNoChangesDoesNotAnimate) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/5);
  ASSERT_EQ(4u, initial_bounds.size());

  GetContinueSectionView()->suggestions_container()->Update();
  base::RunLoop().RunUntilIdle();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      GetContinueSectionView()->suggestions_container()->children();
  ASSERT_EQ(4u, container_children.size());
  for (int i = 0; i < 4; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->layer()->GetAnimator()->is_animating());
  }
}

TEST_P(ContinueSectionViewTest, AnimatesWhenLastItemReplaced) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/5);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(3);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[3];
  EXPECT_EQ(initial_bounds[3], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());

  // Verify views for results that are not changing do not animate.
  for (int i : std::set<int>{0, 1, 2}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->GetVisible());
    views::View* new_result_view = container_children[i + new_views_start];
    EXPECT_FALSE(new_result_view->layer()->GetAnimator()->is_animating());
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(new_result_view));
    EXPECT_EQ(1.0f, new_result_view->layer()->opacity());
  }

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(gfx::Rect(), result_view->layer()->clip_rect());
  }
}

TEST_P(ContinueSectionViewTest, AnimatesWhenFirstItemRemoved) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/5);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(0);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[0];
  EXPECT_EQ(initial_bounds[0], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{0, 1, 2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  // Verify views that are expected to slide out.
  for (int i : std::set<int>{1, 2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);

    views::View* result_view = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(result_view));
    const gfx::RectF target_bounds = GetTargetLayerBounds(result_view);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i], target_bounds);
    } else {
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_GT(initial_bounds[i].x(), target_bounds.x());
    }
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(0.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
  }
}

TEST_P(ContinueSectionViewTest, AnimatesWhenSecondItemRemoved) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/5);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(1);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[0];
  EXPECT_EQ(initial_bounds[0], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(1.0f, view_fading_out->layer()->GetTargetOpacity());

  // Verify the view for the result that's not changing does not animate.
  EXPECT_FALSE(container_children[0]->GetVisible());
  views::View* stationary_view = container_children[new_views_start];
  EXPECT_FALSE(stationary_view->layer()->GetAnimator()->is_animating());
  EXPECT_EQ(initial_bounds[0], GetCurrentLayerBounds(stationary_view));
  EXPECT_EQ(1.0f, stationary_view->layer()->opacity());

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{1, 2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  // Verify views that are expected to slide out.
  for (int i : std::set<int>{2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(result_view));
    const gfx::RectF target_bounds = GetTargetLayerBounds(result_view);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i], target_bounds);
    } else {
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_GT(initial_bounds[i].x(), target_bounds.x());
    }
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(0.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(gfx::Rect(), result_view->layer()->clip_rect());
  }
}

TEST_P(ContinueSectionViewTest, AnimatesWhenThirdItemRemoved) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/5);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(2);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[2];
  EXPECT_EQ(initial_bounds[2], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());

  // Verify views for results that are not changing do not animate.
  for (int i : std::set<int>{0, 1}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->GetVisible());
    views::View* new_result_view = container_children[i + new_views_start];
    EXPECT_FALSE(new_result_view->layer()->GetAnimator()->is_animating());
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(new_result_view));
    EXPECT_EQ(1.0f, new_result_view->layer()->opacity());
  }

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  // Verify views that are expected to slide out.
  for (int i : std::set<int>{3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(result_view));
    const gfx::RectF target_bounds = GetTargetLayerBounds(result_view);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i], target_bounds);
    } else {
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_GT(initial_bounds[i].x(), target_bounds.x());
    }
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(0.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(gfx::Rect(), result_view->layer()->clip_rect());
  }
}

TEST_P(ContinueSectionViewTest, AnimatesWhenItemInserted) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/4);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->AddAt(
      1, CreateTestResult("id5", AppListSearchResultType::kZeroStateDrive,
                          "new result"));

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[3];
  EXPECT_EQ(initial_bounds[3], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());

  // Verify views for results that are not changing do not animate.
  for (int i : std::set<int>{0}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->GetVisible());
    views::View* new_result_view = container_children[i + new_views_start];
    EXPECT_FALSE(new_result_view->layer()->GetAnimator()->is_animating());
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(new_result_view));
    EXPECT_EQ(1.0f, new_result_view->layer()->opacity());
  }

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{1, 2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    if (tablet_mode_param() && i > 1) {
      EXPECT_GT(initial_bounds[i].x(), current_bounds.x());
    } else {
      EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    }
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  // Verify views that are expected to slide out.
  for (int i : std::set<int>{1, 2}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(result_view));
    const gfx::RectF target_bounds = GetTargetLayerBounds(result_view);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i], target_bounds);
    } else {
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_GT(initial_bounds[i].x(), target_bounds.x());
    }
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(0.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(gfx::Rect(), result_view->layer()->clip_rect());
  }
}

TEST_P(ContinueSectionViewTest, AnimatesWhenTwoItemsRemoved) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/6);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(1);
  GetResults()->DeleteAt(1);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();

  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // Removed views should fade out.
  for (int i : std::set<int>{1, 2}) {
    views::View* view_fading_out = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(view_fading_out));
    EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());
  }

  // Verify views for results that are not changing do not animate.
  for (int i : std::set<int>{0}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->GetVisible());
    views::View* new_result_view = container_children[i + new_views_start];
    EXPECT_FALSE(new_result_view->layer()->GetAnimator()->is_animating());
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(new_result_view));
    EXPECT_EQ(1.0f, new_result_view->layer()->opacity());
  }

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{1, 2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  // Verify views that are expected to slide out.
  for (int i : std::set<int>{3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(result_view));
    const gfx::RectF target_bounds = GetTargetLayerBounds(result_view);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i], target_bounds);
    } else {
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_GT(initial_bounds[i].x(), target_bounds.x());
    }
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(0.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 4; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(gfx::Rect(), result_view->layer()->clip_rect());
  }
}

TEST_P(ContinueSectionViewTest, ResultRemovedMidAnimation) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/6);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(1);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  // Remove another result after update (which triggers an update animation).
  GetResults()->DeleteAt(1);
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 4, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[1];
  EXPECT_EQ(initial_bounds[1], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());

  // Verify views for results that are not changing do not animate.
  for (int i : std::set<int>{0}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    EXPECT_FALSE(container_children[i]->GetVisible());
    views::View* new_result_view = container_children[i + new_views_start];
    EXPECT_FALSE(new_result_view->layer()->GetAnimator()->is_animating());
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(new_result_view));
    EXPECT_EQ(1.0f, new_result_view->layer()->opacity());
  }

  // Verify views that are expected to slide in.
  for (int i : std::set<int>{1, 2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i + new_views_start];
    EXPECT_EQ(initial_bounds[i], GetTargetLayerBounds(result_view));
    const gfx::RectF current_bounds = GetCurrentLayerBounds(result_view);
    EXPECT_EQ(initial_bounds[i].size(), current_bounds.size());
    EXPECT_LT(initial_bounds[i].x(), current_bounds.x());
    EXPECT_EQ(0.0f, result_view->layer()->opacity());
    EXPECT_EQ(1.0f, result_view->layer()->GetTargetOpacity());
  }

  // Verify views that are expected to slide out.
  for (int i : std::set<int>{2, 3}) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = container_children[i];
    EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(result_view));
    const gfx::RectF target_bounds = GetTargetLayerBounds(result_view);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i], target_bounds);
    } else {
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_GT(initial_bounds[i].x(), target_bounds.x());
    }
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(0.0f, result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(4u, container_children.size());

  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
    EXPECT_EQ(gfx::Rect(), result_view->layer()->clip_rect());
  }
}

TEST_P(ContinueSectionViewTest, ContinueSectionHiddenMidAnimation) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/4);
  ASSERT_EQ(4u, initial_bounds.size());

  // Remove one item to trigger update animation.
  GetResults()->DeleteAt(1);
  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  // Remove two extra results which should hide the continue section.
  GetResults()->DeleteAt(0);
  GetResults()->DeleteAt(0);
  container_view->Update();

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  ASSERT_EQ(1u, container_children.size());
  EXPECT_FALSE(container_children[0]->layer()->GetAnimator()->is_animating());
}

TEST_P(ContinueSectionViewTest, AnimatesWhenNumberOfChipsChanges) {
  std::vector<gfx::RectF> initial_bounds =
      InitializeForAnimationTest(/*result_count=*/4);
  ASSERT_EQ(4u, initial_bounds.size());

  GetResults()->DeleteAt(2);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  std::vector<raw_ptr<views::View, VectorExperimental>> container_children =
      container_view->children();
  const size_t new_views_start = 4;
  ASSERT_EQ(new_views_start + 3, container_children.size());

  // The removed view should fade out.
  views::View* view_fading_out = container_children[2];
  EXPECT_EQ(initial_bounds[2], GetCurrentLayerBounds(view_fading_out));
  EXPECT_EQ(0.0f, view_fading_out->layer()->GetTargetOpacity());

  // In tablet mode, all views animate when the number of items shown in the
  // container changes.
  if (tablet_mode_param()) {
    for (int i : std::set<int>{0, 1, 3}) {
      SCOPED_TRACE(testing::Message() << "Item at " << i);
      const bool result_after_removed = i > 2;
      views::View* new_result_view =
          container_children[i + new_views_start -
                             (result_after_removed ? 1 : 0)];
      const gfx::RectF current_bounds = GetCurrentLayerBounds(new_result_view);
      EXPECT_EQ(initial_bounds[i].y(), current_bounds.y());
      EXPECT_LT(initial_bounds[i].width(), current_bounds.width());
      const gfx::RectF new_view_target_bounds =
          GetTargetLayerBounds(new_result_view);
      EXPECT_EQ(current_bounds.size(), new_view_target_bounds.size());
      // Items after removed item should animate from the right, and items
      // before the removed item should animate from the left.
      if (i > 1) {
        EXPECT_LT(new_view_target_bounds.x(), current_bounds.x());
      } else {
        EXPECT_GT(new_view_target_bounds.x(), current_bounds.x());
      }
      EXPECT_EQ(0.0f, new_result_view->layer()->opacity());
      EXPECT_EQ(1.0f, new_result_view->layer()->GetTargetOpacity());

      views::View* old_result_view = container_children[i];
      EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(old_result_view));
      const gfx::RectF target_bounds = GetTargetLayerBounds(old_result_view);
      EXPECT_EQ(initial_bounds[i].size(), target_bounds.size());
      EXPECT_EQ(initial_bounds[i].x(), target_bounds.x());
      EXPECT_EQ(1.0f, old_result_view->layer()->opacity());
      EXPECT_EQ(0.0f, old_result_view->layer()->GetTargetOpacity());
    }
  } else {
    // Views before removed one should not animate.
    for (int i : std::set<int>{0, 1}) {
      SCOPED_TRACE(testing::Message() << "Item at " << i);
      EXPECT_FALSE(container_children[i]->GetVisible());
      views::View* new_result_view = container_children[new_views_start + i];
      EXPECT_FALSE(new_result_view->layer()->GetAnimator()->is_animating());
      EXPECT_EQ(initial_bounds[i], GetCurrentLayerBounds(new_result_view));
      EXPECT_EQ(1.0f, new_result_view->layer()->opacity());
    }

    // The last new view should slide in.
    views::View* last_new_result_view = container_children[new_views_start + 2];
    EXPECT_EQ(initial_bounds[2], GetTargetLayerBounds(last_new_result_view));
    const gfx::RectF last_old_current_bounds =
        GetCurrentLayerBounds(last_new_result_view);
    EXPECT_EQ(initial_bounds[2].size(), last_old_current_bounds.size());
    EXPECT_LT(initial_bounds[2].x(), last_old_current_bounds.x());
    EXPECT_EQ(0.0f, last_new_result_view->layer()->opacity());
    EXPECT_EQ(1.0f, last_new_result_view->layer()->GetTargetOpacity());

    // The last old view should slide out.
    views::View* last_old_result_view = container_children[3];
    EXPECT_EQ(initial_bounds[3], GetCurrentLayerBounds(last_old_result_view));
    const gfx::RectF last_old_target_bounds =
        GetTargetLayerBounds(last_old_result_view);
    EXPECT_EQ(initial_bounds[3].size(), last_old_target_bounds.size());
    EXPECT_GT(initial_bounds[3].x(), last_old_target_bounds.x());
    EXPECT_EQ(1.0f, last_old_result_view->layer()->opacity());
    EXPECT_EQ(0.0f, last_old_result_view->layer()->GetTargetOpacity());
  }

  WaitForAllChildrenAnimationsToComplete(container_view);
  VerifyResultViewsUpdated();
  container_children = container_view->children();
  EXPECT_EQ(3u, container_children.size());

  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "Item at " << i);
    views::View* result_view = GetResultViewAt(i);
    if (tablet_mode_param()) {
      EXPECT_EQ(initial_bounds[i].y(), result_view->layer()->bounds().y());
    } else {
      EXPECT_EQ(initial_bounds[i], gfx::RectF(result_view->layer()->bounds()));
    }
    EXPECT_EQ(gfx::Transform(), result_view->layer()->transform());
    EXPECT_EQ(1.0f, result_view->layer()->opacity());
  }
}

TEST_F(ContinueSectionViewClamshellModeTest, AnimatesOutAfterRemovingResults) {
  ResetPrivacyNoticePref();
  InitializeForAnimationTest(/*result_count=*/3);

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  views::View* privacy_notice =
      GetContinueSectionView()->GetPrivacyNoticeForTest();

  RemoveSearchResultAt(1);

  ContinueTaskContainerView* const container_view =
      GetContinueSectionView()->suggestions_container();
  container_view->Update();

  EXPECT_EQ(1.0f, privacy_notice->layer()->opacity());
  EXPECT_EQ(0.0f, privacy_notice->layer()->GetTargetOpacity());
  EXPECT_TRUE(privacy_notice->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter waiter;
  waiter.Wait(privacy_notice->layer());

  VerifyResultViewsUpdated();

  ASSERT_LE(GetContinueSectionView()->GetTasksSuggestionsCount(), 2u);
  EXPECT_FALSE(IsPrivacyNoticeVisible());

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

TEST_P(ContinueSectionViewTest, AnimatesPrivacyNoticeAccept) {
  ResetPrivacyNoticePref();
  InitializeForAnimationTest(/*result_count=*/3);

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  AppListToastView* privacy_notice =
      GetContinueSectionView()->GetPrivacyNoticeForTest();

  GestureTapOn(privacy_notice->toast_button());

  EXPECT_EQ(1.0f, privacy_notice->layer()->opacity());
  EXPECT_EQ(0.0f, privacy_notice->layer()->GetTargetOpacity());
  EXPECT_TRUE(privacy_notice->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter waiter;
  waiter.Wait(privacy_notice->layer());

  ContinueTaskContainerView* container_view =
      GetContinueSectionView()->suggestions_container();

  EXPECT_TRUE(container_view->layer()->GetAnimator()->is_animating());
  WaitForAllChildrenAnimationsToComplete(container_view);
  EXPECT_FALSE(IsPrivacyNoticeVisible());

  EXPECT_TRUE(GetContinueSectionView()->GetVisible());
}

// Regression test for https://crbug.com/1326237.
TEST_F(ContinueSectionViewClamshellModeTest,
       RemoveSearchResultWhileAnimatingContinueSection) {
  ResetPrivacyNoticePref();
  InitializeForAnimationTest(/*result_count=*/3);

  EXPECT_TRUE(IsPrivacyNoticeVisible());
  EXPECT_EQ(GetAppListNudgeController()->current_nudge(),
            AppListNudgeController::NudgeType::kPrivacyNotice);

  AppListToastView* privacy_notice =
      GetContinueSectionView()->GetPrivacyNoticeForTest();

  GestureTapOn(privacy_notice->toast_button());

  EXPECT_EQ(1.0f, privacy_notice->layer()->opacity());
  EXPECT_EQ(0.0f, privacy_notice->layer()->GetTargetOpacity());
  EXPECT_TRUE(privacy_notice->layer()->GetAnimator()->is_animating());

  RemoveSearchResultAt(0);

  EXPECT_EQ(0.0f, privacy_notice->layer()->GetTargetOpacity());
  EXPECT_TRUE(privacy_notice->layer()->GetAnimator()->is_animating());

  ui::LayerAnimationStoppedWaiter waiter;
  waiter.Wait(privacy_notice->layer());

  WaitForAllChildrenAnimationsToComplete(
      GetContinueSectionView()->suggestions_container());
  EXPECT_FALSE(IsPrivacyNoticeVisible());

  EXPECT_FALSE(GetContinueSectionView()->GetVisible());
}

// Regression test for https://crbug.com/1357434.
TEST_F(ContinueSectionViewClamshellModeTest,
       CloseLauncherWhileAnimatingPrivacyToastDoesNotCrash) {
  ResetPrivacyNoticePref();
  InitializeForAnimationTest(/*result_count=*/3);
  ASSERT_TRUE(IsPrivacyNoticeVisible());

  AppListToastView* privacy_toast =
      GetContinueSectionView()->GetPrivacyNoticeForTest();
  ASSERT_TRUE(privacy_toast);

  // Tap on the "OK" button to start the toast dismiss animation.
  GestureTapOn(privacy_toast->toast_button());
  EXPECT_TRUE(privacy_toast->layer()->GetAnimator()->is_animating());

  HideLauncher();
  // No crash.
}

}  // namespace
}  // namespace ash
