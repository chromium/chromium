// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_presenter_impl.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_view_delegate.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/test/apps_grid_view_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr int kAppListBezelMargin = 50;

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void SetShelfAlignment(ShelfAlignment alignment) {
  AshTestBase::GetPrimaryShelf()->SetAlignment(alignment);
}

void EnableTabletMode(bool enable) {
  // Avoid |TabletModeController::OnGetSwitchStates| from disabling tablet mode
  // again at the end of |TabletModeController::TabletModeController|.
  base::RunLoop().RunUntilIdle();

  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enable);

  // The app list will be shown automatically when tablet mode is enabled (Home
  // launcher flag is enabled). Wait here for the animation complete.
  base::RunLoop().RunUntilIdle();
}

// Generates a fling.
void FlingUpOrDown(ui::test::EventGenerator* generator,
                   AppListView* view,
                   bool up) {
  int offset = up ? -100 : 100;
  gfx::Point start_point = view->GetBoundsInScreen().origin();
  gfx::Point target_point = start_point;
  target_point.Offset(0, offset);

  generator->GestureScrollSequence(start_point, target_point,
                                   base::TimeDelta::FromMilliseconds(10), 2);
}

}  // namespace

class AppListPresenterDelegateZeroStateTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AppListPresenterDelegateZeroStateTest() = default;
  ~AppListPresenterDelegateZeroStateTest() override = default;

  // testing::Test:
  void SetUp() override {
    AppListView::SetShortAnimationForTesting(true);
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");
  }

  // testing::Test:
  void TearDown() override {
    AshTestBase::TearDown();
    AppListView::SetShortAnimationForTesting(false);
  }

  // Whether to run the test with mouse or gesture events.
  bool TestMouseEventParam() { return GetParam(); }

  gfx::Point GetPointOutsideSearchbox() {
    return GetAppListView()->GetBoundsInScreen().origin();
  }

  gfx::Point GetPointInsideSearchbox() {
    return GetAppListView()
        ->search_box_view()
        ->GetBoundsInScreen()
        .CenterPoint();
  }

  AppListView* GetAppListView() {
    return GetAppListTestHelper()->GetAppListView();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateZeroStateTest);
};

class AppListPresenterDelegateTest
    : public AppListPresenterDelegateZeroStateTest {
 public:
  AppListPresenterDelegateTest() {
    // Zeros state changes expected UI behavior. Most test cases in this suite
    // are the expected UI behavior with zero state being disabled.
    // TODO(jennyz): Add new test cases for zero state, crbug.com/925195.
    scoped_feature_list_.InitAndDisableFeature(
        app_list_features::kEnableZeroStateSuggestions);
  }
  ~AppListPresenterDelegateTest() override = default;

 protected:
  void SetAppListStateAndWait(AppListViewState new_state) {
    GetAppListView()->SetState(new_state);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(new_state);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateTest);
};

// Used to test app_list behavior with a populated apps_grid
class PopulatedAppListTest : public AshTestBase,
                             public testing::WithParamInterface<bool> {
 public:
  PopulatedAppListTest() = default;
  ~PopulatedAppListTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Make the display big enough to hold the app list.
    UpdateDisplay("1024x768");

    app_list_test_delegate_ = std::make_unique<test::AppListTestViewDelegate>();

    app_list_test_model_ = app_list_test_delegate_->GetTestModel();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    AppListView::SetShortAnimationForTesting(false);
  }

 protected:
  void CreateAndOpenAppList() {
    app_list_view_ = new AppListView(app_list_test_delegate_.get());
    app_list_view_->InitView(
        false /*is_tablet_mode*/, CurrentContext(),
        base::BindRepeating(&UpdateActivationForAppListView, app_list_view_,
                            /*is_tablet_mode=*/false));
    app_list_view_->Show(false /*is_side_shelf*/, false /*is_tablet_mode*/);
  }

  void ShowAppListInAppsFullScreen() {
    // Press the ExpandArrowView and check that the AppListView is in
    // fullscreen.
    gfx::Point click_point = app_list_view_->app_list_main_view()
                                 ->contents_view()
                                 ->expand_arrow_view()
                                 ->GetBoundsInScreen()
                                 .CenterPoint();
    GetEventGenerator()->GestureTapAt(click_point);
    EXPECT_EQ(AppListViewState::kFullscreenAllApps,
              app_list_view_->app_list_state());
  }

  void InitializeAppsGrid() {
    if (!app_list_view_)
      CreateAndOpenAppList();
    apps_grid_view_ = app_list_view_->app_list_main_view()
                          ->contents_view()
                          ->GetAppsContainerView()
                          ->apps_grid_view();
    apps_grid_test_api_ =
        std::make_unique<test::AppsGridViewTestApi>(apps_grid_view_);
  }
  gfx::Rect GetItemRectOnCurrentPageAt(int row, int col) const {
    DCHECK_GT(app_list_test_model_->top_level_item_list()->item_count(), 0u);
    return apps_grid_test_api_->GetItemTileRectOnCurrentPageAt(row, col);
  }

  test::AppListTestModel* app_list_test_model_ = nullptr;
  std::unique_ptr<test::AppsGridViewTestApi> apps_grid_test_api_;
  std::unique_ptr<test::AppListTestViewDelegate> app_list_test_delegate_;
  AppListView* app_list_view_ = nullptr;    // Owned by native widget.
  AppsGridView* apps_grid_view_ = nullptr;  // Owned by |app_list_view_|.
};

// Subclass of PopuplatedAppListTest which enables the animation and the virtual
// keyboard.
class PopulatedAppListWithVKEnabledTest : public PopulatedAppListTest {
 public:
  PopulatedAppListWithVKEnabledTest() = default;
  ~PopulatedAppListWithVKEnabledTest() override = default;

  void SetUp() override {
    AppListView::SetShortAnimationForTesting(true);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    PopulatedAppListTest::SetUp();
  }
};

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(, AppListPresenterDelegateTest, testing::Bool());

// Verifies that context menu click should not activate the search box
// (see https://crbug.com/941428).
TEST_F(AppListPresenterDelegateZeroStateTest, RightClickSearchBoxInPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list_view = GetAppListView();
  gfx::Rect app_list_bounds = app_list_view->GetBoundsInScreen();
  ASSERT_EQ(AppListViewState::kPeeking, app_list_view->app_list_state());

  // Right click the search box and checks the following things:
  // (1) AppListView's bounds in screen does not change.
  // (2) AppListView is still in Peeking state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointInsideSearchbox());
  generator->PressRightButton();
  EXPECT_EQ(app_list_bounds, app_list_view->GetBoundsInScreen());
  EXPECT_EQ(AppListViewState::kPeeking, app_list_view->app_list_state());
}

TEST_F(AppListPresenterDelegateZeroStateTest,
       ReshownAppListResetsSearchBoxActivation) {
  // Activate the search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetEventGenerator()->GestureTapAt(GetPointInsideSearchbox());

  // Dismiss and re-show the AppList.
  GetAppListTestHelper()->Dismiss();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Test that the search box is no longer active.
  EXPECT_FALSE(GetAppListTestHelper()
                   ->GetAppListView()
                   ->search_box_view()
                   ->is_search_box_active());
}

// Tests that the SearchBox activation is reset after the AppList is hidden with
// no animation from FULLSCREEN_SEARCH.
TEST_F(AppListPresenterDelegateZeroStateTest,
       SideShelfAppListResetsSearchBoxActivationOnClose) {
  // Set the shelf to one side, then show the AppList and activate the
  // searchbox.
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_RIGHT);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetEventGenerator()->GestureTapAt(GetPointInsideSearchbox());
  ASSERT_TRUE(GetAppListTestHelper()
                  ->GetAppListView()
                  ->search_box_view()
                  ->is_search_box_active());

  // Dismiss the AppList using the controller, this is the same way we dismiss
  // the AppList when a SearchResult is launched, and skips the
  // FULLSCREEN_SEARCH -> FULLSCREEN_ALL_APPS transition.
  Shell::Get()->app_list_controller()->DismissAppList();

  // Test that the search box is not active.
  EXPECT_FALSE(GetAppListTestHelper()
                   ->GetAppListView()
                   ->search_box_view()
                   ->is_search_box_active());
}

// Verifies that tapping on the search box in tablet mode with animation and
// zero state enabled should not bring Chrome crash (https://crbug.com/958267).
TEST_F(AppListPresenterDelegateZeroStateTest, ClickSearchBoxInTabletMode) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Necessary for AppListView::StateAnimationMetricsReporter::Report being
  // called when animation ends.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  AppListView::SetShortAnimationForTesting(false);

  ui::test::EventGenerator* generator = GetEventGenerator();

  // Gesture tap on the search box.
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Wait until animation finishes. Verifies AppListView's state.
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Gesture tap on the area out of search box.
  generator->GestureTapAt(GetPointOutsideSearchbox());

  // Wait until animation finishes. Verifies AppListView's state.
  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Verifies that the downward mouse drag on AppsGridView's first page should
// be handled by AppList.
TEST_F(PopulatedAppListTest, MouseDragAppsGridViewHandledByAppList) {
  InitializeAppsGrid();
  app_list_test_model_->PopulateApps(2);
  ShowAppListInAppsFullScreen();

  // Calculate the drag start/end points.
  gfx::Point drag_start_point = apps_grid_view_->GetBoundsInScreen().origin();
  gfx::Point target_point = GetPrimaryDisplay().bounds().bottom_left();
  target_point.set_x(drag_start_point.x());

  // Drag AppsGridView downward by mouse. Check the following things:
  // (1) Mouse events are processed by AppsGridView, including mouse press,
  // mouse drag and mouse release.
  // (2) AppList is closed after mouse drag.
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(drag_start_point);
  event_generator->DragMouseTo(target_point);
  event_generator->ReleaseLeftButton();

  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

// Verifies that the upward mouse drag on AppsGridView's first page should
// be handled by PaginationController.
TEST_F(PopulatedAppListTest,
       MouseDragAppsGridViewHandledByPaginationController) {
  InitializeAppsGrid();
  app_list_test_model_->PopulateApps(apps_grid_test_api_->TilesPerPage(0) + 1);
  EXPECT_EQ(2, apps_grid_view_->pagination_model()->total_pages());
  ShowAppListInAppsFullScreen();

  // Calculate the drag start/end points. |drag_start_point| is between the
  // first and the second AppListItem. Because in this test case, we want
  // AppsGridView to receive mouse events instead of AppListItemView.
  gfx::Point right_side =
      apps_grid_view_->GetItemViewAt(0)->GetBoundsInScreen().right_center();
  gfx::Point left_side =
      apps_grid_view_->GetItemViewAt(1)->GetBoundsInScreen().left_center();
  ASSERT_EQ(left_side.y(), right_side.y());
  gfx::Point drag_start_point((right_side.x() + left_side.x()) / 2,
                              right_side.y());
  gfx::Point target_point = GetPrimaryDisplay().bounds().top_right();
  target_point.set_x(drag_start_point.x());

  // Drag AppsGridView downward by mouse. Checks that PaginationController
  // records the mouse drag.
  base::HistogramTester histogram_tester;
  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(drag_start_point);
  event_generator->DragMouseTo(target_point);
  event_generator->ReleaseLeftButton();
  histogram_tester.ExpectUniqueSample(
      kAppListPageSwitcherSourceHistogramInClamshell,
      AppListPageSwitcherSource::kMouseDrag, 1);
}

TEST_F(PopulatedAppListWithVKEnabledTest,
       TappingAppsGridClosesVirtualKeyboard) {
  InitializeAppsGrid();
  app_list_test_model_->PopulateApps(2);
  gfx::Point between_apps = GetItemRectOnCurrentPageAt(0, 0).right_center();
  gfx::Point empty_space = GetItemRectOnCurrentPageAt(0, 2).CenterPoint();

  ui::GestureEvent tap_between(between_apps.x(), between_apps.y(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ui::GestureEvent tap_outside(empty_space.x(), empty_space.y(), 0,
                               base::TimeTicks(),
                               ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true /* locked */);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Touch the apps_grid outside of any apps
  apps_grid_view_->OnGestureEvent(&tap_outside);
  // Expect that the event is ignored here and allowed to propogate to app_list
  EXPECT_FALSE(tap_outside.handled());
  // Hit the app_list with the same event
  app_list_view_->OnGestureEvent(&tap_outside);
  // Expect that the event is handled and the keyboard is closed.
  EXPECT_TRUE(tap_outside.handled());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // Reshow the VKeyboard
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Touch the apps_grid between two apps
  apps_grid_view_->OnGestureEvent(&tap_between);
  // Expect the event to be handled in the grid, and the keyboard to be closed.
  EXPECT_TRUE(tap_between.handled());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
}

// Tests that app list hides when focus moves to a normal window.
TEST_F(AppListPresenterDelegateTest, HideOnFocusOut) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that app list remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_F(AppListPresenterDelegateTest,
       RemainVisibleWhenFocusingToApplistContainer) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests opening the app list on a secondary display, then deleting the display.
TEST_F(AppListPresenterDelegateTest, NonPrimaryDisplay) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  ASSERT_EQ("1024,0 1024x768", root_windows[1]->GetBoundsInScreen().ToString());

  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  // Remove the secondary display. Shouldn't crash (http://crbug.com/368990).
  UpdateDisplay("1024x768");

  // Updating the displays should close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests updating display should not close the app list.
TEST_F(AppListPresenterDelegateTest, UpdateDisplayNotCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Change display bounds.
  UpdateDisplay("1024x768");

  // Updating the display should not close the app list.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests the app list window's bounds under multi-displays environment.
TEST_F(AppListPresenterDelegateTest, AppListWindowBounds) {
  // Set up a screen with two displays (horizontally adjacent).
  UpdateDisplay("1024x768,1024x768");
  const gfx::Size display_size(1024, 768);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Test the app list window's bounds on primary display.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect primary_display_rect(
      gfx::Point(0, display_size.height() -
                        AppListConfig::instance().peeking_app_list_height()),
      display_size);
  EXPECT_EQ(
      primary_display_rect,
      GetAppListView()->GetWidget()->GetNativeView()->GetBoundsInScreen());

  // Close the app list on primary display.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);

  // Test the app list window's bounds on secondary display.
  GetAppListTestHelper()->ShowAndRunLoop(GetSecondaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect secondary_display_rect(
      gfx::Point(display_size.width(),
                 display_size.height() -
                     AppListConfig::instance().peeking_app_list_height()),
      display_size);
  EXPECT_EQ(
      secondary_display_rect,
      GetAppListView()->GetWidget()->GetNativeView()->GetBoundsInScreen());
}

// Tests that the app list window's bounds and the search box bounds are updated
// when the display bounds change.
TEST_F(AppListPresenterDelegateTest, AppListBoundsChangeForDisplayChange) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the half
// state are updated when the display bounds change.
TEST_F(AppListPresenterDelegateTest,
       AppListBoundsChangeForDisplayChangeSearch) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kHalf);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the
// fullscreen state are updated when the display bounds change.
TEST_F(AppListPresenterDelegateTest,
       AppListBoundsChangeForDisplayChangeFullscreen) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kFullscreenAllApps);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list window's bounds and the search box bounds in the
// fullscreen search state are updated when the display bounds change.
TEST_F(AppListPresenterDelegateTest,
       AppListBoundsChangeForDisplayChangeFullscreenSearch) {
  UpdateDisplay("1024x768");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  SetAppListStateAndWait(AppListViewState::kFullscreenAllApps);
  SetAppListStateAndWait(AppListViewState::kFullscreenSearch);

  const gfx::Rect app_list_bounds =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds = GetAppListView()
                                          ->search_box_view()
                                          ->GetWidget()
                                          ->GetWindowBoundsInScreen();

  UpdateDisplay("800x600");
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  const gfx::Rect app_list_bounds2 =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen();
  const gfx::Rect search_box_bounds2 = GetAppListView()
                                           ->search_box_view()
                                           ->GetWidget()
                                           ->GetWindowBoundsInScreen();
  EXPECT_GT(app_list_bounds.size().GetArea(),
            app_list_bounds2.size().GetArea());
  EXPECT_NE(search_box_bounds, search_box_bounds2);
  EXPECT_EQ(400, search_box_bounds2.CenterPoint().x());
}

// Tests that the app list is not draggable in side shelf alignment.
TEST_F(AppListPresenterDelegateTest, SideShelfAlignmentDragDisabled) {
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_RIGHT);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  const AppListView* app_list = GetAppListView();
  EXPECT_TRUE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Drag the widget across the screen over an arbitrary 100Ms, this would
  // normally result in the app list transitioning to PEEKING but will now
  // result in no state change.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(GetPointOutsideSearchbox(),
                                   gfx::Point(10, 900),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Tap the app list body. This should still close the app list.
  generator->GestureTapAt(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the app list initializes in fullscreen with side shelf alignment
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, SideShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  SetShelfAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);

  // Open the app list with side shelf alignment, then check that it is in
  // fullscreen mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list = GetAppListView();
  EXPECT_TRUE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list initializes in peeking with bottom shelf alignment
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, BottomShelfAlignmentTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list = GetAppListView();
  EXPECT_FALSE(app_list->is_fullscreen());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter text in the searchbox, this should transition the app list to half
  // state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Empty the searchbox, this should transition the app list to it's previous
  // state.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
}

// Tests that the app list initializes in fullscreen with tablet mode active
// and that the state transitions via text input act properly.
TEST_F(AppListPresenterDelegateTest, TabletModeTextStateTransitions) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Delete the text in the searchbox, the app list should transition to
  // fullscreen all apps.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list closes when tablet mode deactivates.
TEST_F(AppListPresenterDelegateTest, AppListClosesWhenLeavingTabletMode) {
  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);

  EnableTabletMode(true);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kFullscreenSearch);

  EnableTabletMode(false);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(ash::AppListViewState::kClosed);
}

// Tests that the app list state responds correctly to tablet mode being
// enabled while the app list is being shown with half launcher.
TEST_F(AppListPresenterDelegateTest, HalfToFullscreenWhenTabletModeIsActive) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Enable tablet mode and force the app list to transition to the fullscreen
  // equivalent of the current state.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list view handles drag properly in laptop mode.
TEST_F(AppListPresenterDelegateTest, AppListViewDragHandler) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  ui::test::EventGenerator* generator = GetEventGenerator();
  // Execute a slow short upwards drag this should fail to transition the app
  // list.
  int top_of_app_list =
      GetAppListView()->GetWidget()->GetWindowBoundsInScreen().y();
  generator->GestureScrollSequence(gfx::Point(0, top_of_app_list + 20),
                                   gfx::Point(0, top_of_app_list - 20),
                                   base::TimeDelta::FromMilliseconds(500), 50);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Execute a long upwards drag, this should transition the app list.
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Execute a short downward drag, this should fail to transition the app list.
  gfx::Point start(10, 10);
  gfx::Point end(10, 100);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Execute a long and slow downward drag to switch to peeking.
  start = gfx::Point(10, 200);
  end = gfx::Point(10, 650);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Transition to fullscreen.
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text to transition to |FULLSCREEN_SEARCH|.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Execute a short downward drag, this should fail to close the app list.
  start = gfx::Point(10, 10);
  end = gfx::Point(10, 100);
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end, 1, 100),
      100);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Execute a long downward drag, this should close the app list.
  generator->GestureScrollSequence(gfx::Point(10, 10), gfx::Point(10, 900),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the bottom shelf background is hidden when the app list is shown
// in laptop mode.
TEST_F(AppListPresenterDelegateTest,
       ShelfBackgroundIsHiddenWhenAppListIsShown) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests the shelf background type is as expected when a window is created after
// going to tablet mode.
TEST_F(AppListPresenterDelegateTest, ShelfBackgroundWithHomeLauncher) {
  // Enter tablet mode to display the home launcher.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);
  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_HOME_LAUNCHER,
            shelf_layout_manager->GetShelfBackgroundType());

  // Add a window. It should be maximized because it is in tablet mode.
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests that app list understands shelf rounded corners state while animating
// out and in, and that it keeps getting notified of shelf state changes if
// close animation is interrupted by another show request.
TEST_F(AppListPresenterDelegateTest, AppListShownWhileClosing) {
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  // Enable animation to account for delay between app list starting to close
  // and reporting visibility change (which happens when close animation
  // finishes).
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  AppListView::SetShortAnimationForTesting(false);

  // Dismiss and immediately show the app list (before close animation is done).
  GetAppListTestHelper()->Dismiss();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Finish app list animations.
  ASSERT_TRUE(
      GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->is_animating());
  GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->StopAnimating();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  // Verify that the app list still picks up shelf changes.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_TRUE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests how shelf state is updated as app list state changes with a maximized
// window open. It verifies that the app list knows that the maximized shelf had
// no rounded corners.
TEST_F(AppListPresenterDelegateTest, AppListWithMaximizedShelf) {
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  // Enable animation to account for delay between app list starting to close
  // and reporting visibility change (which happens when close animation
  // finishes).
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  AppListView::SetShortAnimationForTesting(false);

  // Start closing the app list view.
  GetAppListTestHelper()->Dismiss();

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  // Minimize the window, and verify that the shelf state changed from a
  // maximized state, and that |shelf_has_rounded_corners()| value was updated.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);

  EXPECT_TRUE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  // Stop app list hide animation.
  ASSERT_TRUE(
      GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->is_animating());
  GetAppListView()->GetWidget()->GetLayer()->GetAnimator()->StopAnimating();

  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Verifies the shelf background state changes when a window is maximized while
// app list is shown. Verifies that AppList::shelf_has_rounded_corners() is
// updated.
TEST_F(AppListPresenterDelegateTest, WindowMaximizedWithAppListShown) {
  auto window = CreateTestWindow();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  ShelfLayoutManager* shelf_layout_manager =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()))
          ->shelf_layout_manager();

  EXPECT_TRUE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  EXPECT_FALSE(GetAppListView()->shelf_has_rounded_corners());
  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());

  GetAppListTestHelper()->Dismiss();

  EXPECT_EQ(ShelfBackgroundType::SHELF_BACKGROUND_MAXIMIZED,
            shelf_layout_manager->GetShelfBackgroundType());
}

// Tests that the bottom shelf is auto hidden when a window is fullscreened in
// tablet mode (home launcher is shown behind).
TEST_F(AppListPresenterDelegateTest, ShelfAutoHiddenWhenFullscreen) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);
  Shelf* shelf =
      Shelf::ForWindow(Shell::GetRootWindowForDisplayId(GetPrimaryDisplayId()));
  EXPECT_EQ(ShelfVisibilityState::SHELF_VISIBLE, shelf->GetVisibilityState());

  // Create and fullscreen a window. The shelf should be auto hidden.
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(ShelfVisibilityState::SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN,
            shelf->GetAutoHideState());
}

// Tests that the peeking app list closes if the user taps or clicks outside
// its bounds.
TEST_P(AppListPresenterDelegateTest, TapAndClickOutsideClosesPeekingAppList) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Tapping outside the bounds closes the app list.
  const gfx::Rect peeking_bounds = GetAppListView()->GetBoundsInScreen();
  gfx::Point tap_point = peeking_bounds.origin();
  tap_point.Offset(10, -10);
  ASSERT_FALSE(peeking_bounds.Contains(tap_point));

  if (test_mouse_event) {
    generator->MoveMouseTo(tap_point);
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(tap_point);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterDelegateTest, LongPressOutsideCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point = GetAppListView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch LONG_PRESS to AppListPresenterDelegate.
  ui::TouchEvent long_press(
      ui::ET_GESTURE_LONG_PRESS, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  GetEventGenerator()->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_P(AppListPresenterDelegateTest, TwoFingerTapOutsideCloseAppList) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // |outside_point| is outside the bounds of app list.
  gfx::Point outside_point = GetAppListView()->bounds().origin();
  outside_point.Offset(0, -10);

  // Dispatch TWO_FINGER_TAP to AppListPresenterDelegate.
  ui::TouchEvent two_finger_tap(
      ui::ET_GESTURE_TWO_FINGER_TAP, outside_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH));
  GetEventGenerator()->Dispatch(&two_finger_tap);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a keypress activates the searchbox and that clearing the
// searchbox, the searchbox remains active.
TEST_F(AppListPresenterDelegateTest, KeyPressEnablesSearchBox) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator* generator = GetEventGenerator();
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Press any key, the search box should be active.
  generator->PressKey(ui::VKEY_0, 0);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Delete the text, the search box should be inactive.
  search_box_view->ClearSearch();
  EXPECT_TRUE(search_box_view->is_search_box_active());
}

// Tests that a tap/click on the AppListView from half launcher returns the
// AppListView to Peeking, and that a tap/click on the AppListView from
// Peeking closes the app list.
TEST_P(AppListPresenterDelegateTest,
       StateTransitionsByTapAndClickingAppListBodyFromHalf) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list_view = GetAppListView();
  SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Press a key, the AppListView should transition to half.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the search box, the AppListView should transition to Peeking
  // and the search box should be inactive.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the search box again, the AppListView should hide.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a tap/click on the AppListView from Fullscreen search returns
// the AppListView to fullscreen all apps, and that a tap/click on the
// AppListView from fullscreen all apps closes the app list.
TEST_P(AppListPresenterDelegateTest,
       StateTransitionsByTappingAppListBodyFromFullscreen) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* app_list_view = GetAppListView();
  SearchBoxView* search_box_view = app_list_view->search_box_view();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Execute a long upwards drag, this should transition the app list to
  // fullscreen.
  const int top_of_app_list =
      app_list_view->GetWidget()->GetWindowBoundsInScreen().y();
  generator->GestureScrollSequence(gfx::Point(10, top_of_app_list + 20),
                                   gfx::Point(10, 10),
                                   base::TimeDelta::FromMilliseconds(100), 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Press a key, this should activate the searchbox and transition to
  // fullscreen search.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap outside the searchbox, this should deactivate the searchbox and the
  // applistview should return to fullscreen all apps.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(search_box_view->is_search_box_active());

  // Tap outside the searchbox again, this should close the applistview.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
  } else {
    generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the searchbox activates when it is tapped and that the widget is
// closed after tapping outside the searchbox.
TEST_P(AppListPresenterDelegateTest, TapAndClickEnablesSearchBox) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  SearchBoxView* search_box_view = GetAppListView()->search_box_view();

  // Tap/Click the search box, it should activate.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointInsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointInsideSearchbox());
  }

  EXPECT_TRUE(search_box_view->is_search_box_active());

  // Tap on the body of the app list, the search box should deactivate.
  if (test_mouse_event) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(search_box_view->is_search_box_active());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap on the body of the app list again, the app list should hide.
  if (test_mouse_event) {
    generator->PressLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the shelf background displays/hides with bottom shelf
// alignment.
TEST_F(AppListPresenterDelegateTest,
       ShelfBackgroundRespondsToAppListBeingShown) {
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_BOTTOM);

  // Show the app list, the shelf background should be transparent.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  EXPECT_EQ(SHELF_BACKGROUND_APP_LIST,
            shelf_layout_manager->GetShelfBackgroundType());
  GetAppListTestHelper()->DismissAndRunLoop();

  // Set the alignment to the side and show the app list. The background
  // should show.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::SHELF_ALIGNMENT_LEFT);
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_FALSE(GetPrimaryShelf()->IsHorizontalAlignment());
  EXPECT_EQ(
      SHELF_BACKGROUND_APP_LIST,
      GetPrimaryShelf()->shelf_layout_manager()->GetShelfBackgroundType());
}

// Tests that the half app list closes itself if the user taps outside its
// bounds.
TEST_P(AppListPresenterDelegateTest, TapAndClickOutsideClosesHalfAppList) {
  // TODO(newcomer): Investigate mash failures crbug.com/726838
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Transition to half app list by entering text.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // A point outside the bounds of launcher.
  gfx::Point to_point(
      0, GetAppListView()->GetWidget()->GetWindowBoundsInScreen().y() - 1);

  // Clicking/tapping outside the bounds closes the app list.
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(to_point);
    generator->ClickLeftButton();
  } else {
    generator->GestureTapAt(to_point);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the search box is set active with a whitespace query and that the
// app list state doesn't transition with a whitespace query.
TEST_F(AppListPresenterDelegateTest, WhitespaceQuery) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  ui::test::EventGenerator* generator = GetEventGenerator();
  EXPECT_FALSE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter a whitespace query, the searchbox should activate but stay in peeking
  // mode.
  generator->PressKey(ui::VKEY_SPACE, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter a non-whitespace character, the Searchbox should stay active and go
  // to HALF
  generator->PressKey(ui::VKEY_0, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  // Delete the non whitespace character, the Searchbox should not deactivate
  // but go to PEEKING
  generator->PressKey(ui::VKEY_BACK, 0);
  EXPECT_TRUE(view->search_box_view()->is_search_box_active());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
}

// Tests that an unhandled two finger tap/right click does not close the app
// list, and an unhandled one finger tap/left click closes the app list in
// Peeking mode.
TEST_P(AppListPresenterDelegateTest, UnhandledEventOnPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Two finger tap or right click in the empty space below the searchbox. The
  // app list should not close.
  gfx::Point empty_space =
      GetAppListView()->search_box_view()->GetBoundsInScreen().bottom_left();
  empty_space.Offset(0, 10);
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(empty_space);
    generator->PressRightButton();
    generator->ReleaseRightButton();
  } else {
    ui::GestureEvent two_finger_tap(
        empty_space.x(), empty_space.y(), 0, base::TimeTicks(),
        ui::GestureEventDetails(ui::ET_GESTURE_TWO_FINGER_TAP));
    generator->Dispatch(&two_finger_tap);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);

  // One finger tap or left click in the empty space below the searchbox. The
  // app list should close.
  if (TestMouseEventParam()) {
    generator->MoveMouseTo(empty_space);
    generator->ClickLeftButton();
  } else {
    generator->GestureTapAt(empty_space);
  }
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a drag to the bezel from Fullscreen/Peeking will close the app
// list.
TEST_P(AppListPresenterDelegateTest,
       DragToBezelClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  }

  // Drag the app list to 50 DIPs from the bottom bezel.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int bezel_y = display::Screen::GetScreen()
                          ->GetDisplayNearestView(view->parent_window())
                          .bounds()
                          .bottom();
  generator->GestureScrollSequence(
      gfx::Point(0, bezel_y - (kAppListBezelMargin + 100)),
      gfx::Point(0, bezel_y - (kAppListBezelMargin)),
      base::TimeDelta::FromMilliseconds(1500), 100);

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that a fling from Fullscreen/Peeking closes the app list.
TEST_P(AppListPresenterDelegateTest,
       FlingDownClosesAppListFromFullscreenAndPeeking) {
  const bool test_fullscreen = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  if (test_fullscreen) {
    FlingUpOrDown(GetEventGenerator(), view, true /* up */);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  }

  // Fling down, the app list should close.
  FlingUpOrDown(GetEventGenerator(), view, false /* down */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that drag using a mouse does not always close the app list if the app
// list was previously closed using a fling gesture.
TEST_P(AppListPresenterDelegateTest, MouseDragAfterDownwardFliing) {
  const bool test_fullscreen = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  AppListView* view = GetAppListView();
  const gfx::Point expand_arrow_point = view->app_list_main_view()
                                            ->contents_view()
                                            ->expand_arrow_view()
                                            ->GetBoundsInScreen()
                                            .CenterPoint();

  if (test_fullscreen)
    GetEventGenerator()->GestureTapAt(expand_arrow_point);
  GetAppListTestHelper()->CheckState(test_fullscreen
                                         ? AppListViewState::kFullscreenAllApps
                                         : AppListViewState::kPeeking);

  // Fling down, the app list should close.
  FlingUpOrDown(GetEventGenerator(), view, false /* down */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);

  // Show the app list again, and perform mouse drag that ends up at the same
  // position.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  if (test_fullscreen)
    GetEventGenerator()->GestureTapAt(expand_arrow_point);
  GetAppListTestHelper()->CheckState(test_fullscreen
                                         ? AppListViewState::kFullscreenAllApps
                                         : AppListViewState::kPeeking);

  GetEventGenerator()->MoveMouseTo(GetPointOutsideSearchbox());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseBy(0, -10);
  GetEventGenerator()->MoveMouseBy(0, 10);
  GetEventGenerator()->ReleaseLeftButton();

  // Verify the app list state has not changed.
  GetAppListTestHelper()->CheckState(test_fullscreen
                                         ? AppListViewState::kFullscreenAllApps
                                         : AppListViewState::kPeeking);
}

TEST_F(AppListPresenterDelegateTest,
       MouseWheelFromAppListPresenterImplTransitionsAppListState) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  GetAppListView()->HandleScroll(gfx::Vector2d(0, -30), ui::ET_MOUSEWHEEL);

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_P(AppListPresenterDelegateTest, LongUpwardDragInFullscreenShouldNotClose) {
  const bool test_fullscreen_search = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();
  FlingUpOrDown(GetEventGenerator(), view, true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  if (test_fullscreen_search) {
    // Enter a character into the searchbox to transition to FULLSCREEN_SEARCH.
    GetEventGenerator()->PressKey(ui::VKEY_0, 0);
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  }

  // Drag from the center of the applist to the top of the screen very slowly.
  // This should not trigger a state transition.
  gfx::Point drag_start = view->GetBoundsInScreen().CenterPoint();
  drag_start.set_x(15);
  gfx::Point drag_end = view->GetBoundsInScreen().top_right();
  drag_end.set_x(15);
  GetEventGenerator()->GestureScrollSequence(
      drag_start, drag_end,
      GetEventGenerator()->CalculateScrollDurationForFlingVelocity(
          drag_start, drag_end, 1, 1000),
      1000);
  GetAppListTestHelper()->WaitUntilIdle();
  if (test_fullscreen_search)
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  else
    GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests closing the app list during drag, and verifies the bounds get properly
// updated when the app list is shown again..
TEST_P(AppListPresenterDelegateTest, CloseAppListDuringDrag) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point drag_start = GetAppListView()->GetBoundsInScreen().origin();

  // Start drag and press escape to close the app list view.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(drag_start);
    generator->PressLeftButton();
    generator->MoveMouseBy(0, -10);
  } else {
    generator->MoveTouch(drag_start);
    generator->PressTouch();
    generator->MoveTouch(drag_start + gfx::Vector2d(0, -10));
  }

  EXPECT_TRUE(GetAppListView()->is_in_drag());
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  EXPECT_FALSE(GetAppListView()->is_in_drag());

  // Show the app list and verify the app list returns to peeking position.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_EQ(drag_start, GetAppListView()->GetBoundsInScreen().origin());
}

// Tests closing the app list during drag, and verifies that drag updates are
// ignored while the app list is closing.
TEST_P(AppListPresenterDelegateTest, DragUpdateWhileAppListClosing) {
  const bool test_mouse_event = TestMouseEventParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point drag_start = GetAppListView()->GetBoundsInScreen().origin();

  // Set up non zero animation duration to ensure app list is not closed
  // immediately.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  AppListView::SetShortAnimationForTesting(false);

  // Start drag and press escape to close the app list view.
  ui::test::EventGenerator* generator = GetEventGenerator();
  if (test_mouse_event) {
    generator->MoveMouseTo(drag_start);
    generator->PressLeftButton();
    generator->MoveMouseBy(0, -10);
  } else {
    generator->MoveTouch(drag_start);
    generator->PressTouch();
    generator->MoveTouch(drag_start + gfx::Vector2d(0, -10));
  }
  EXPECT_TRUE(GetAppListView()->is_in_drag());

  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);

  // Update the drag before running the loop that waits for the close animation
  // to finish,
  if (test_mouse_event) {
    generator->MoveMouseBy(0, -10);
  } else {
    generator->MoveTouch(drag_start + gfx::Vector2d(0, -20));
  }

  base::RunLoop().RunUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  EXPECT_FALSE(GetAppListView()->is_in_drag());

  // Show the app list and verify the app list returns to peeking position.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_EQ(drag_start, GetAppListView()->GetBoundsInScreen().origin());
}

// Tests that a drag can not make the app list smaller than the shelf height.
TEST_F(AppListPresenterDelegateTest, LauncherCannotGetSmallerThanShelf) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  AppListView* view = GetAppListView();

  // Try to place the app list 1 px below the shelf, it should stay at shelf
  // height.
  int target_y = GetPrimaryShelf()
                     ->GetShelfViewForTesting()
                     ->GetBoundsInScreen()
                     .top_right()
                     .y();
  const int expected_app_list_y = target_y;
  target_y += 1;
  view->UpdateYPositionAndOpacity(target_y, 1);

  EXPECT_EQ(expected_app_list_y, view->GetBoundsInScreen().top_right().y());
}

// Tests that the AppListView is on screen on a small display.
TEST_F(AppListPresenterDelegateTest, SearchBoxShownOnSmallDisplay) {
  // Update the display to a small scale factor.
  UpdateDisplay("600x400");
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Animate to Half.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());

  // Animate to peeking.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());

  // Animate back to Half.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(0, view->GetWidget()->GetNativeView()->bounds().y());
}

// Tests that the AppListView is on screen on a small work area.
TEST_F(AppListPresenterDelegateTest, SearchBoxShownOnSmallWorkArea) {
  // Update the work area to a small size.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  ASSERT_TRUE(display_manager()->UpdateWorkAreaOfDisplay(
      GetPrimaryDisplayId(), gfx::Insets(400, 0, 0, 0)));

  // Animate to Half.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  AppListView* view = GetAppListView();
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());

  // Animate to peeking.
  generator->PressKey(ui::KeyboardCode::VKEY_BACK, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());

  // Animate back to Half.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);
  EXPECT_LE(GetPrimaryDisplay().work_area().y(),
            view->GetWidget()->GetNativeView()->bounds().y());
}

// Tests that no crash occurs after an attempt to show app list in an invalid
// display.
TEST_F(AppListPresenterDelegateTest, ShowInInvalidDisplay) {
  GetAppListTestHelper()->ShowAndRunLoop(display::kInvalidDisplayId);
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
}

// Tests that tap the auto-hide shelf with app list opened should dismiss the
// app list but keep shelf visible.
TEST_F(AppListPresenterDelegateTest, TapAutoHideShelfWithAppListOpened) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a normal unmaximized window; the shelf should be hidden.
  std::unique_ptr<views::Widget> window = CreateTestWidget();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap the system tray should open system tray bubble and keep shelf visible.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap to dismiss the app list and the auto-hide shelf.
  generator->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  // Show the AppList again.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Test that tapping the auto-hidden shelf keeps the app list and shelf
  // visible.
  generator->GestureTapAt(
      shelf->GetShelfViewForTesting()->GetBoundsInScreen().CenterPoint());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Verifies that in clamshell mode, AppList has the expected state based on the
// drag distance after dragging from Peeking state.
TEST_F(AppListPresenterDelegateTest, DragAppListViewFromPeeking) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Calculate |threshold| in the same way with AppListView::EndDrag.
  const int threshold = AppListConfig::instance().peeking_app_list_height() /
                        kAppListThresholdDenominator;

  // Drag AppListView downward by |threshold| then release the gesture.
  // Check the final state should be Peeking.
  ui::test::EventGenerator* generator = GetEventGenerator();
  AppListView* view = GetAppListView();
  const int drag_to_peeking_distance = threshold;
  gfx::Point drag_start = view->GetBoundsInScreen().top_center();
  gfx::Point drag_end(drag_start.x(),
                      drag_start.y() + drag_to_peeking_distance);
  generator->GestureScrollSequence(
      drag_start, drag_end,
      generator->CalculateScrollDurationForFlingVelocity(drag_start, drag_end,
                                                         2, 1000),
      1000);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Drag AppListView upward by bigger distance then release the gesture.
  // Check the final state should be kFullscreenAllApps.
  const int drag_to_fullscreen_distance = threshold + 1;
  drag_start = view->GetBoundsInScreen().top_center();
  drag_end =
      gfx::Point(drag_start.x(), drag_start.y() - drag_to_fullscreen_distance);

  generator->GestureScrollSequence(
      drag_start, drag_end,
      generator->CalculateScrollDurationForFlingVelocity(drag_start, drag_end,
                                                         2, 1000),
      1000);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests that the app list background corner radius remains constant during app
// list drag if the shelf is not in maximized state.
TEST_F(AppListPresenterDelegateTest, BackgroundCornerRadiusDuringDrag) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  const gfx::Point shelf_top = GetPrimaryShelf()
                                   ->GetShelfViewForTesting()
                                   ->GetBoundsInScreen()
                                   .top_center();
  const int background_radius = ShelfConfig::Get()->shelf_size() / 2;

  AppListView* view = GetAppListView();
  const views::View* const background_shield =
      view->GetAppListBackgroundShieldForTest();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Start drag at the peeking height, and move to different
  // positions relative to the shelf top.
  // Verify that the app list background shield never changes.
  const gfx::Point peeking_top = view->GetBoundsInScreen().top_center();
  generator->MoveTouch(peeking_top);
  generator->PressTouch();
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the shelf, with an offset less than the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the top of the shelf.
  generator->MoveTouch(shelf_top);
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to half rounded background radius height.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 2));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height just under the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius - 1));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height that equals the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height just over the background radius.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 1));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the peeking height.
  generator->MoveTouch(gfx::Point(peeking_top.x(), peeking_top.y() + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move back to peeking height, and end drag.
  generator->MoveTouch(peeking_top);
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
}

// Tests how app list background rounded corners are changed during a drag while
// the shelf is in a maximized state (i.e. while a maximized window is shown).
TEST_F(AppListPresenterDelegateTest,
       BackgroundCornerRadiusDuringDragWithMaximizedShelf) {
  auto window = CreateTestWindow();
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  const gfx::Point shelf_top = GetPrimaryShelf()
                                   ->GetShelfViewForTesting()
                                   ->GetBoundsInScreen()
                                   .top_center();
  const int background_radius = ShelfConfig::Get()->shelf_size() / 2;

  AppListView* view = GetAppListView();
  const views::View* const background_shield =
      view->GetAppListBackgroundShieldForTest();
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Start drag at the peeking app list top.
  const gfx::Point peeking_top = view->GetBoundsInScreen().top_center();
  generator->MoveTouch(peeking_top);
  generator->PressTouch();

  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the shelf, with an offset less than the background radius.
  // Verify that current background corner radius matches the offset from the
  // shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 5));
  EXPECT_EQ(
      gfx::RoundedCornersF(background_radius / 5, background_radius / 5, 0, 0),
      background_shield->layer()->rounded_corner_radii());

  // Move to the shelf top - background should have no rounded corners.
  generator->MoveTouch(shelf_top);
  EXPECT_EQ(gfx::RoundedCornersF(),
            background_shield->layer()->rounded_corner_radii());

  // Move to half background radius height - the background corner radius should
  // match the offset from the shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius / 2));
  EXPECT_EQ(
      gfx::RoundedCornersF(background_radius / 2, background_radius / 2, 0, 0),
      background_shield->layer()->rounded_corner_radii());

  // Move to the height just under the background radius - the current
  // background corners should be equal to the offset from the shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius - 1));
  EXPECT_EQ(
      gfx::RoundedCornersF(background_radius - 1, background_radius - 1, 0, 0),
      background_shield->layer()->rounded_corner_radii());

  // Move to the height that equals the background radius - the current
  // background corners should be equal to the offset from the shelf.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move to the height just over the background radius - the background corner
  // radius value should stay at the |background_radius| value.
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 1));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
  generator->MoveTouch(shelf_top - gfx::Vector2d(0, background_radius + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move above the peeking height - the background radius should remain the
  // same.
  generator->MoveTouch(gfx::Point(peeking_top.x(), peeking_top.y() + 5));
  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());

  // Move back to peeking height, and end drag.
  generator->MoveTouch(peeking_top);
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  EXPECT_EQ(gfx::RoundedCornersF(background_radius, background_radius, 0, 0),
            background_shield->layer()->rounded_corner_radii());
}

// Tests for variety of app list behavior with kScalableAppList feature enabled
// or disabled, depending on the parameter value.
class AppListPresenterDelegateScalableAppListTest
    : public AppListPresenterDelegateTest {
 public:
  AppListPresenterDelegateScalableAppListTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {app_list_features::kScalableAppList,
           ash::features::kEnableBackgroundBlur},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {ash::features::kEnableBackgroundBlur},
          {app_list_features::kScalableAppList});
    }
  }

  void SetUp() override {
    AppListPresenterDelegateTest::SetUp();

    UpdateDisplay("1080x900");
    GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  }

  bool ScalableAppListEnabled() const { return GetParam(); }

  int ExpectedSuggestionChipContainerTop(const gfx::Rect& search_box_bounds) {
    return search_box_bounds.bottom() +
           (ScalableAppListEnabled() ? 16 : 24); /*suggesion chip top margin*/
  }

  // Calculates expected apps grid position based on display height and the
  // search box in-screen bounds.
  // NOTE: This assumes that the display size is such that the preferred apps
  // grid size is within min and max apps grid height (in which case the margin
  // when scalable app list is not enabled is 1 / 16 of the available height).
  int ExpectedAppsGridTop(const AppListConfig& config,
                          int display_height,
                          const gfx::Rect& search_box_bounds) {
    if (ScalableAppListEnabled()) {
      return ExpectedSuggestionChipContainerTop(search_box_bounds) +
             32 /*suggestion chip container height*/ +
             config.grid_fadeout_zone_height() -
             config.grid_fadeout_mask_height();
    }

    int top_with_margin =
        ExpectedSuggestionChipContainerTop(search_box_bounds) +
        32 /*suggestion chip container height*/;
    const int available_height =
        display_height - ShelfConfig::Get()->shelf_size() -
        config.search_box_fullscreen_top_padding() -
        search_box_bounds.height() - 32 /*suggestion chip container height*/
        - 24 /*margin between suggestion chip and search box*/;
    return top_with_margin + (available_height) / 16 -
           config.grid_fadeout_zone_height();
  }

  // Calculates expected apps grid position on the search results page based on
  // the display height and the search box in-screen bounds.
  int ExpectedAppsGridTopForSearchResults(const AppListConfig& config,
                                          int display_height,
                                          const gfx::Rect& search_box_bounds) {
    const int top =
        ExpectedAppsGridTop(config, display_height, search_box_bounds);
    // In the search results page, the apps grid is shown 24 dip below where
    // they'd be shown in the apps page. The |top| was calculated relative to
    // search box bounds in the search results page, so it has to be further
    // offset by the difference between search box bottom bounds in the apps and
    // search results page.
    const int search_box_diff =
        contents_view()
            ->GetSearchBoxBounds(ash::AppListState::kStateApps)
            .bottom() -
        contents_view()
            ->GetSearchBoxBounds(ash::AppListState::kStateSearchResults)
            .bottom();
    return top + search_box_diff +
           24 /*apps grid offset in fullscreen search state*/;
  }

  ContentsView* contents_view() {
    return GetAppListView()->app_list_main_view()->contents_view();
  }

  AppsGridView* apps_grid_view() {
    return GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->GetAppsContainerView()
        ->apps_grid_view();
  }

  SearchResultPageView* search_result_page() {
    return GetAppListView()
        ->app_list_main_view()
        ->contents_view()
        ->search_results_page_view();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateScalableAppListTest);
};

// The parameter indicates whether the kScalableAppList feature is enabled.
INSTANTIATE_TEST_SUITE_P(ScalableAppListEnabled,
                         AppListPresenterDelegateScalableAppListTest,
                         testing::Bool());

// Tests that the app list contents top margin is gradually updated during drag
// between peeking and fullscreen view state while showing apps page.
TEST_P(AppListPresenterDelegateScalableAppListTest,
       AppsPagePositionDuringDrag) {
  const AppListConfig& config = GetAppListView()->GetAppListConfig();
  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int fullscreen_y = 0;
  const int closed_y = 900 - shelf_height;
  const int fullscreen_search_box_padding =
      ScalableAppListEnabled() ? (900 - shelf_height) / 16
                               : config.search_box_fullscreen_top_padding();

  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point peeking_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  // Drag AppListView upwards half way to the top of the screen, and check the
  // search box padding has been updated to a value half-way between peeking and
  // fullscreen values.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(peeking_top);
  generator->PressTouch();
  generator->MoveTouch(
      gfx::Point(peeking_top.x(), (peeking_top.y() + fullscreen_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  float progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagNone);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);

  EXPECT_EQ((peeking_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1, config.search_box_peeking_top_padding(),
                    fullscreen_search_box_padding),
            search_box_bounds.y());

  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  // In apps state, search results page should be hidden behind the search
  // box.
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Move to the fullscreen position, and verify the search box padding is
  // equal to the expected fullscreen value.
  generator->MoveTouch(gfx::Point(peeking_top.x(), fullscreen_y));
  GetAppListTestHelper()->WaitUntilIdle();

  EXPECT_EQ(2.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagNone));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(fullscreen_search_box_padding, search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Move half way between peeking and closed state - the search box padding
  // should be half distance between closed and peeking padding.
  generator->MoveTouch(
      gfx::Point(peeking_top.x(), (peeking_top.y() + closed_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagNone);
  EXPECT_LE(std::abs(progress - 0.5f), 0.01f);

  EXPECT_EQ((peeking_top.y() + closed_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress, config.search_box_peeking_top_padding(),
                    config.search_box_closed_top_padding()),
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Move to the closed state height, and verify the search box padding matches
  // the state.
  generator->MoveTouch(gfx::Point(peeking_top.x(), closed_y));
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(0.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagNone));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(closed_y + config.search_box_closed_top_padding(),
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());
}

// Tests that the app list contents top margin is gradually updated during drag
// between half and fullscreen state while showing search results.
TEST_P(AppListPresenterDelegateScalableAppListTest,
       SearchResultsPagePositionDuringDrag) {
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  const AppListConfig& config = GetAppListView()->GetAppListConfig();
  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int search_results_height = 440;
  const int fullscreen_y = 0;
  const int closed_y = 900 - shelf_height;
  const int fullscreen_search_box_padding =
      ScalableAppListEnabled() ? (900 - shelf_height) / 16
                               : config.search_box_fullscreen_top_padding();

  const gfx::Point half_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  // Drag AppListView upwards half way to the top of the screen, and check the
  // search box padding has been updated to a value half-way between peeking and
  // fullscreen values.
  generator->MoveTouch(half_top);
  generator->PressTouch();
  generator->MoveTouch(
      gfx::Point(half_top.x(), (half_top.y() + fullscreen_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  float progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);

  EXPECT_EQ((half_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1, config.search_box_fullscreen_top_padding(),
                    fullscreen_search_box_padding),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  // Move to the fullscreen position, and verify the search box padding is
  // equal to the expected fullscreen value.
  generator->MoveTouch(gfx::Point(half_top.x(), fullscreen_y));
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(2.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagSearchResults));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(fullscreen_search_box_padding, search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  // Move half way between peeking and closed state - the search box padding
  // should be half distance between closed and peeking padding.
  generator->MoveTouch(gfx::Point(half_top.x(), (half_top.y() + closed_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 0.5f), 0.01f);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ((half_top.y() + closed_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress, config.search_box_fullscreen_top_padding(),
                    config.search_box_closed_top_padding()),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  // Move to the closed state height, and verify the search box padding matches
  // the state.
  generator->MoveTouch(gfx::Point(half_top.x(), closed_y));
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(0.0f, GetAppListView()->GetAppListTransitionProgress(
                      AppListView::kProgressFlagSearchResults));

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(closed_y + config.search_box_closed_top_padding(),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_TRUE(apps_grid_view()->GetVisible());
}

// Tests changing the active app list page while drag is in progress.
TEST_P(AppListPresenterDelegateScalableAppListTest, SwitchPageDuringDrag) {
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  const gfx::Point peeking_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  const gfx::Point half_top =
      GetAppListView()->GetBoundsInScreen().top_center();

  const AppListConfig& config = GetAppListView()->GetAppListConfig();
  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int search_results_height = 440;
  const int fullscreen_y = 0;
  const int fullscreen_search_box_padding =
      ScalableAppListEnabled() ? (900 - shelf_height) / 16
                               : config.search_box_fullscreen_top_padding();

  // Drag AppListView upwards half way to the top of the screen, and check the
  // search box padding has been updated to a value half-way between peeking and
  // fullscreen values.
  generator->MoveTouch(half_top);
  generator->PressTouch();
  generator->MoveTouch(
      gfx::Point(half_top.x(), (half_top.y() + fullscreen_y) / 2));
  GetAppListTestHelper()->WaitUntilIdle();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  float progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);
  EXPECT_EQ((half_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1, config.search_box_fullscreen_top_padding(),
                    fullscreen_search_box_padding),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_TRUE(apps_grid_view()->GetVisible());

  const gfx::Rect apps_grid_bounds_in_results_page =
      apps_grid_view()->GetBoundsInScreen();
  const gfx::Rect app_list_bounds = GetAppListView()->GetBoundsInScreen();

  // Press ESC key - this should move the UI back to the app list.
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // The app list position should remain the same.
  EXPECT_EQ(app_list_bounds, GetAppListView()->GetBoundsInScreen());

  // The search box should be moved so drag progress for peeking state matches
  // the current height.
  float new_progress = (0.5 * half_top.y()) / peeking_top.y();
  int expected_search_box_top =
      new_progress * peeking_top.y() +
      (1 - new_progress) * fullscreen_search_box_padding +
      new_progress * config.search_box_peeking_top_padding();

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(expected_search_box_top, search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(apps_grid_bounds_in_results_page.y() - 24,
              apps_grid_view()->GetBoundsInScreen().y());
  }
  EXPECT_EQ(apps_grid_bounds_in_results_page.size(),
            apps_grid_view()->GetBoundsInScreen().size());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  // Enter text in the search box to transition back to search results page.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kHalf);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  progress = GetAppListView()->GetAppListTransitionProgress(
      AppListView::kProgressFlagSearchResults);
  EXPECT_LE(std::abs(progress - 1.5f), 0.01f);
  EXPECT_EQ((half_top.y() + fullscreen_y) / 2 +
                gfx::Tween::IntValueBetween(
                    progress - 1, config.search_box_fullscreen_top_padding(),
                    fullscreen_search_box_padding),
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_TRUE(apps_grid_view()->GetVisible());
}

// Tests changing the active app list page in fullscreen state.
TEST_P(AppListPresenterDelegateScalableAppListTest, SwitchPageInFullscreen) {
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  const AppListConfig& config = GetAppListView()->GetAppListConfig();
  const int shelf_height = ShelfConfig::Get()->shelf_size();
  const int search_results_height = 440;
  const int fullscreen_y = 0;
  const int fullscreen_search_box_padding =
      ScalableAppListEnabled() ? (900 - shelf_height) / 16
                               : config.search_box_fullscreen_top_padding();

  gfx::Rect search_box_bounds =
      GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(fullscreen_y + fullscreen_search_box_padding,
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());

  const gfx::Rect app_list_bounds = GetAppListView()->GetBoundsInScreen();

  // Enter text in the search box to transition to half app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(app_list_bounds, GetAppListView()->GetBoundsInScreen());
  EXPECT_EQ(fullscreen_y + fullscreen_search_box_padding,
            search_box_bounds.y());
  EXPECT_EQ(search_box_bounds.y(),
            search_result_page()->GetBoundsInScreen().y());
  EXPECT_EQ(search_results_height,
            search_result_page()->GetBoundsInScreen().height());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(
        ExpectedAppsGridTopForSearchResults(config, 900, search_box_bounds),
        apps_grid_view()->GetBoundsInScreen().y());
  } else {
    // Apps grid should be off screen.
    EXPECT_GT(apps_grid_view()->GetBoundsInScreen().y(), 900);
  }
  EXPECT_EQ(ScalableAppListEnabled(), apps_grid_view()->GetVisible());
  const gfx::Rect apps_grid_bounds_in_results_page =
      apps_grid_view()->GetBoundsInScreen();

  // Press ESC key - this should move the UI back to the app list.
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  search_box_bounds = GetAppListView()->search_box_view()->GetBoundsInScreen();
  search_box_bounds.Inset(GetAppListView()->search_box_view()->GetInsets());

  EXPECT_EQ(app_list_bounds, GetAppListView()->GetBoundsInScreen());
  EXPECT_EQ(fullscreen_y + fullscreen_search_box_padding,
            search_box_bounds.y());
  EXPECT_EQ(ExpectedAppsGridTop(config, 900, search_box_bounds),
            apps_grid_view()->GetBoundsInScreen().y());
  EXPECT_TRUE(apps_grid_view()->GetVisible());
  if (ScalableAppListEnabled()) {
    EXPECT_EQ(apps_grid_bounds_in_results_page.y() - 24,
              apps_grid_view()->GetBoundsInScreen().y());
  }
  EXPECT_EQ(apps_grid_bounds_in_results_page.size(),
            apps_grid_view()->GetBoundsInScreen().size());
  EXPECT_EQ(search_box_bounds, search_result_page()->GetBoundsInScreen());
}

// Test a variety of behaviors for home launcher (app list in tablet mode).
// Parameterized on whether gesture navigation flags are enabled.
class AppListPresenterDelegateHomeLauncherTest
    : public AppListPresenterDelegateTest {
 public:
  AppListPresenterDelegateHomeLauncherTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kEnableBackgroundBlur,
           features::kDragFromShelfToHomeOrOverview},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kEnableBackgroundBlur},
          {features::kDragFromShelfToHomeOrOverview});
    }
  }
  ~AppListPresenterDelegateHomeLauncherTest() override = default;

  // testing::Test:
  void SetUp() override {
    AppListPresenterDelegateTest::SetUp();
    GetAppListTestHelper()->WaitUntilIdle();
  }

  void PressHomeButton() {
    Shell::Get()->app_list_controller()->OnHomeButtonPressed(
        GetPrimaryDisplayId(), AppListShowSource::kShelfButton,
        base::TimeTicks());
    GetAppListTestHelper()->WaitUntilIdle();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(AppListPresenterDelegateHomeLauncherTest);
};

INSTANTIATE_TEST_SUITE_P(,
                         AppListPresenterDelegateHomeLauncherTest,
                         testing::Bool());

// Verifies that mouse dragging AppListView is enabled.
TEST_P(AppListPresenterDelegateHomeLauncherTest, MouseDragAppList) {
  std::unique_ptr<AppListItem> item(new AppListItem("fake id"));
  Shell::Get()->app_list_controller()->GetModel()->AddItem(std::move(item));

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Drag AppListView upward by mouse. Before moving the mouse, AppsGridView
  // should be invisible.
  const gfx::Point start_point = GetAppListView()->GetBoundsInScreen().origin();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_point);
  generator->PressLeftButton();
  AppsGridView* apps_grid_view = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->GetAppsContainerView()
                                     ->apps_grid_view();
  EXPECT_FALSE(apps_grid_view->GetVisible());

  // Verifies that the AppListView state after mouse drag should be
  // FullscreenAllApps.
  generator->MoveMouseBy(0, -start_point.y());
  generator->ReleaseLeftButton();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_TRUE(apps_grid_view->GetVisible());
}

// Verifies that mouse dragging AppListView creates layers, causes to change the
// opacity, and destroys the layers when done.
TEST_P(AppListPresenterDelegateHomeLauncherTest, MouseDragAppListItemOpacity) {
  const int items_in_page = AppListConfig::instance().preferred_cols() *
                            AppListConfig::instance().preferred_rows();
  for (int i = 0; i < items_in_page; ++i) {
    std::unique_ptr<AppListItem> item(
        new AppListItem(base::StringPrintf("fake id %d", i)));
    Shell::Get()->app_list_controller()->GetModel()->AddItem(std::move(item));
  }

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  // Drag AppListView by mouse. Before moving the mouse, each AppListItem
  // doesn't have its own layer.
  const gfx::Point start_point = GetAppListView()->GetBoundsInScreen().origin();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_point);
  generator->PressLeftButton();
  AppsGridView* apps_grid_view = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->GetAppsContainerView()
                                     ->apps_grid_view();
  // No items have layer.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }

  // Drags the mouse a bit above (twice as shelf's height). This should show the
  // item vaguely.
  const int shelf_height =
      GetPrimaryShelf()->GetShelfViewForTesting()->height();
  generator->MoveMouseBy(0, -shelf_height * 2);
  // All of the item should have the layer at this point.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Moves the mouse to the top edge of the screen; now all app-list items are
  // fully visible, but stays to keep layer. The opacity should be almost 1.0.
  generator->MoveMouseTo(start_point.x(), 0);
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Finishes the drag. It should destruct the layer.
  generator->ReleaseLeftButton();
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that ending of the mouse dragging of app-list destroys the layers for
// the items which are in the second page. See https://crbug.com/990529.
TEST_P(AppListPresenterDelegateHomeLauncherTest, LayerOnSecondPage) {
  const int items_in_page = AppListConfig::instance().preferred_cols() *
                            AppListConfig::instance().preferred_rows();
  AppListModel* model = Shell::Get()->app_list_controller()->GetModel();
  for (int i = 0; i < items_in_page; ++i) {
    std::unique_ptr<AppListItem> item(
        new AppListItem(base::StringPrintf("fake id %02d", i)));
    model->AddItem(std::move(item));
  }

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);

  const gfx::Point start_point = GetAppListView()->GetBoundsInScreen().origin();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_point);
  generator->PressLeftButton();
  AppsGridView* apps_grid_view = GetAppListView()
                                     ->app_list_main_view()
                                     ->contents_view()
                                     ->GetAppsContainerView()
                                     ->apps_grid_view();

  // Drags the mouse a bit above (twice as shelf's height). This should show the
  // item vaguely.
  const int shelf_height =
      GetPrimaryShelf()->GetShelfViewForTesting()->height();
  generator->MoveMouseBy(0, -shelf_height * 2);
  // All of the item should have the layer at this point.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Add items at the front of the items.
  const int additional_items = 10;
  syncer::StringOrdinal prev_position =
      model->top_level_item_list()->item_at(0)->position();
  for (int i = 0; i < additional_items; ++i) {
    std::unique_ptr<AppListItem> item(
        new AppListItem(base::StringPrintf("fake id %02d", i + items_in_page)));
    // Update the position so that the item is added at the front of the list.
    auto metadata = item->CloneMetadata();
    metadata->position = prev_position.CreateBefore();
    prev_position = metadata->position;
    item->SetMetadata(std::move(metadata));
    model->AddItem(std::move(item));
  }

  generator->MoveMouseBy(0, -1);

  // At this point, some items move out from the first page.
  EXPECT_LT(1, apps_grid_view->pagination_model()->total_pages());

  // The items on the first page should have layers.
  for (int i = 0; i < items_in_page; ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_TRUE(item_view->layer()) << "at " << i;
    EXPECT_LE(0.f, item_view->layer()->opacity()) << "at " << i;
    EXPECT_GE(1.f, item_view->layer()->opacity()) << "at " << i;
  }

  // Drag to the top of the screen and finish the drag. It should destroy all
  // of the layers, including items on the second page.
  generator->MoveMouseTo(start_point.x(), 0);
  generator->ReleaseLeftButton();
  for (int i = 0; i < apps_grid_view->view_model()->view_size(); ++i) {
    views::View* item_view = apps_grid_view->view_model()->view_at(i);
    EXPECT_FALSE(item_view->layer()) << "at " << i;
  }
}

// Tests that the app list is shown automatically when the tablet mode is on.
// The app list is dismissed when the tablet mode is off.
TEST_P(AppListPresenterDelegateHomeLauncherTest, ShowAppListForTabletMode) {
  GetAppListTestHelper()->CheckVisibility(false);

  // Turns on tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Turns off tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that the app list window's parent is changed after entering tablet
// mode.
TEST_P(AppListPresenterDelegateHomeLauncherTest, ParentWindowContainer) {
  // Show app list in non-tablet mode. The window container should be
  // kShellWindowId_AppListContainer.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  aura::Window* window = GetAppListView()->GetWidget()->GetNativeWindow();
  aura::Window* root_window = window->GetRootWindow();
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_AppListContainer)
                  ->Contains(window));

  // Turn on tablet mode. The window container should be
  // kShellWindowId_HomeScreenContainer.
  EnableTabletMode(true);
  EXPECT_TRUE(root_window->GetChildById(kShellWindowId_HomeScreenContainer)
                  ->Contains(window));
}

// Tests that the background opacity change for app list.
TEST_P(AppListPresenterDelegateHomeLauncherTest, BackgroundOpacity) {
  // Show app list in non-tablet mode. The background sheild opacity should be
  // 70%.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // The opacity should be set on the color, not the layer. Setting opacity on
  // the layer will change the opacity of the blur effect, which is not desired.
  const U8CPU clamshell_background_opacity = static_cast<U8CPU>(255 * 0.74);
  EXPECT_EQ(SkColorSetA(AppListView::kDefaultBackgroundColor,
                        clamshell_background_opacity),
            GetAppListView()->GetAppListBackgroundShieldColorForTest());
  EXPECT_EQ(1, GetAppListView()
                   ->GetAppListBackgroundShieldForTest()
                   ->layer()
                   ->opacity());

  // Turn on tablet mode. The background shield should be transparent.
  EnableTabletMode(true);

  const U8CPU tablet_background_opacity = static_cast<U8CPU>(0);
  EXPECT_EQ(SkColorSetA(AppListView::kDefaultBackgroundColor,
                        tablet_background_opacity),
            GetAppListView()->GetAppListBackgroundShieldColorForTest());
  EXPECT_EQ(1, GetAppListView()
                   ->GetAppListBackgroundShieldForTest()
                   ->layer()
                   ->opacity());
}

// Tests that the background blur which is present in clamshell mode does not
// show in tablet mode.
TEST_P(AppListPresenterDelegateHomeLauncherTest, BackgroundBlur) {
  // Show app list in non-tablet mode. The background blur should be enabled.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_GT(GetAppListView()
                ->GetAppListBackgroundShieldForTest()
                ->layer()
                ->background_blur(),
            0.0f);

  // Turn on tablet mode. The background blur should be disabled.
  EnableTabletMode(true);
  EXPECT_EQ(0.0f, GetAppListView()
                      ->GetAppListBackgroundShieldForTest()
                      ->layer()
                      ->background_blur());
}

// Tests that tapping or clicking on background cannot dismiss the app list.
TEST_P(AppListPresenterDelegateHomeLauncherTest, TapOrClickToDismiss) {
  // Show app list in non-tablet mode. Click outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Tap outside search box.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Click outside search box.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());
  generator->PressLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Tap outside search box.
  generator->GestureTapDownAndUp(GetPointOutsideSearchbox());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that accelerator Escape, Broswer back and Search key cannot dismiss the
// appt list.
TEST_P(AppListPresenterDelegateHomeLauncherTest, PressAcceleratorToDismiss) {
  // Show app list in non-tablet mode. Press Escape key.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Press Browser back key.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in non-tablet mode. Press search key.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Press Escape key.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->PressKey(ui::KeyboardCode::VKEY_ESCAPE, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Press Browser back key.
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_BACK, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Press search key.
  generator->PressKey(ui::KeyboardCode::VKEY_BROWSER_SEARCH, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that moving focus outside app list window can dismiss it.
TEST_P(AppListPresenterDelegateHomeLauncherTest, FocusOutToDismiss) {
  // Show app list in non-tablet mode. Move focus to another window.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Go to tablet mode with a focused window, the AppList should not be visible.
  EnableTabletMode(true);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Refocusing the already focused window should change nothing.
  wm::ActivateWindow(window.get());

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Minimizing the focused window with no remaining windows should result in a
  // shown applist.
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);

  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the gesture-scroll cannot dismiss the app list.
TEST_P(AppListPresenterDelegateHomeLauncherTest, GestureScrollToDismiss) {
  // Show app list in non-tablet mode. Fling down.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), false /* up */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Show app list in tablet mode. Fling down.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  FlingUpOrDown(GetEventGenerator(), GetAppListView(), false /* up */);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the mouse-scroll cannot dismiss the app list.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       MouseScrollDoesntDismissPeekingLauncher) {
  // Show app list in non-tablet mode. Mouse-scroll up.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up to get fullscreen.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);

  // Reset and show app list in non-tablet mode. Mouse-scroll down.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  GetAppListTestHelper()->CheckVisibility(true);

  // Scroll down to get fullscreen.
  generator->MoveMouseWheel(0, -1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that mouse-scroll up at fullscreen will dismiss app list.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       MouseScrollToDismissFromFullscreen) {
  // Show app list in non-tablet mode. Mouse-scroll down.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckState(AppListViewState::kPeeking);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up with mouse wheel to fullscreen.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  GetAppListTestHelper()->CheckVisibility(true);
  generator->MoveMouseTo(GetPointOutsideSearchbox());

  // Scroll up with mouse wheel to close app list.
  generator->MoveMouseWheel(0, 1);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kClosed);
  GetAppListTestHelper()->CheckVisibility(false);
}

// Test that the AppListView opacity is reset after it is hidden during the
// overview mode animation.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       LauncherShowsAfterOverviewMode) {
  // Show the AppList in clamshell mode.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Enable overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();

  // Test that the AppListView is transparent.
  EXPECT_EQ(0.0f, GetAppListView()->GetWidget()->GetLayer()->opacity());

  // Disable overview mode.
  overview_controller->EndOverview();

  // Show the launcher, test that the opacity is restored.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  EXPECT_EQ(1.0f, GetAppListView()->GetWidget()->GetLayer()->opacity());
  EXPECT_TRUE(GetAppListView()->GetWidget()->IsVisible());
}

// Tests that tapping home button while home screen is visible and showing
// search results moves the home screen to apps container page.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       HomeButtonDismissesSearchResults) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  // Enter text in the searchbox, the app list should transition to fullscreen
  // search.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::KeyboardCode::VKEY_0, ui::EF_NONE);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Tap home button - verify that home goes back to showing the apps page.
  PressHomeButton();

  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

// Tests the app list opacity in overview mode.
TEST_P(AppListPresenterDelegateHomeLauncherTest, OpacityInOverviewMode) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Enable overview mode.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ui::Layer* layer = GetAppListView()->GetWidget()->GetNativeWindow()->layer();
  EXPECT_EQ(0.0f, layer->opacity());

  // Disable overview mode.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(1.0f, layer->opacity());
}

// Tests the app list visibility during wallpaper preview.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       VisibilityDuringWallpaperPreview) {
  WallpaperControllerTestApi wallpaper_test_api(
      Shell::Get()->wallpaper_controller());

  // The app list is hidden in the beginning.
  GetAppListTestHelper()->CheckVisibility(false);
  // Open wallpaper picker and start preview. Verify the app list remains
  // hidden.
  wallpaper_test_api.StartWallpaperPreview();
  GetAppListTestHelper()->CheckVisibility(false);
  // Enable tablet mode. Verify the app list is still hidden because wallpaper
  // preview is active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(false);
  // End preview by confirming the wallpaper. Verify the app list is shown.
  wallpaper_test_api.EndWallpaperPreview(true /*confirm_preview_wallpaper=*/);
  GetAppListTestHelper()->CheckVisibility(true);

  // Start preview again. Verify the app list is hidden.
  wallpaper_test_api.StartWallpaperPreview();
  GetAppListTestHelper()->CheckVisibility(false);
  // End preview by canceling the wallpaper. Verify the app list is shown.
  wallpaper_test_api.EndWallpaperPreview(false /*confirm_preview_wallpaper=*/);
  GetAppListTestHelper()->CheckVisibility(true);

  // Start preview again and enable overview mode during the wallpaper preview.
  // Verify the app list is hidden.
  wallpaper_test_api.StartWallpaperPreview();
  EXPECT_FALSE(GetAppListView()->GetWidget()->IsVisible());
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(false);
  // Disable overview mode. Verify the app list is still hidden because
  // wallpaper preview is still active.
  overview_controller->EndOverview();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(false);
  // End preview by confirming the wallpaper. Verify the app list is shown.
  wallpaper_test_api.EndWallpaperPreview(true /*confirm_preview_wallpaper=*/);
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the home button will minimize all windows.
TEST_P(AppListPresenterDelegateHomeLauncherTest, HomeButtonMinimizeAllWindows) {
  // Show app list in tablet mode. Maximize all windows.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0)),
      window2(CreateTestWindowInShellWithId(1)),
      window3(CreateTestWindowInShellWithId(2));
  WindowState *state1 = WindowState::Get(window1.get()),
              *state2 = WindowState::Get(window2.get()),
              *state3 = WindowState::Get(window3.get());
  state1->Maximize();
  state2->Maximize();
  state3->Maximize();
  EXPECT_TRUE(state1->IsMaximized());
  EXPECT_TRUE(state2->IsMaximized());
  EXPECT_TRUE(state3->IsMaximized());

  // The windows need to be activated for the mru window tracker.
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window3.get());
  auto ordering =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  // Press home button.
  PressHomeButton();
  EXPECT_TRUE(state1->IsMinimized());
  EXPECT_TRUE(state2->IsMinimized());
  EXPECT_TRUE(state3->IsMinimized());
  GetAppListTestHelper()->CheckVisibility(true);

  // Tests that the window ordering remains the same as before we minimize.
  auto new_order =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  EXPECT_TRUE(std::equal(ordering.begin(), ordering.end(), new_order.begin()));
}

// Tests that the home button will end split view mode.
TEST_P(AppListPresenterDelegateHomeLauncherTest, HomeButtonEndSplitViewMode) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Press home button.
  PressHomeButton();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the home button will end overview mode.
TEST_P(AppListPresenterDelegateHomeLauncherTest, HomeButtonEndOverviewMode) {
  // Show app list in tablet mode. Enter overview mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Press home button.
  PressHomeButton();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the home button will end overview and split view mode if both are
// active (e.g. one side of the split view contains overview).
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       HomeButtonEndSplitViewModeWithOverview) {
  // Show app list in tablet mode. Enter split view mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> dummy_window(CreateTestWindowInShellWithId(1));

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Press home button.
  PressHomeButton();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that the context menu is triggered in the same way as if we are on
// the wallpaper.
TEST_P(AppListPresenterDelegateHomeLauncherTest, WallpaperContextMenu) {
  // Show app list in tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Long press on the app list to open the context menu.
  // TODO(ginko) look into a way to populate an apps grid, then get a point
  // between these apps so that clicks/taps between apps can be tested
  const gfx::Point onscreen_point(GetPointOutsideSearchbox());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ui::GestureEvent long_press(
      onscreen_point.x(), onscreen_point.y(), 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_LONG_PRESS));
  generator->Dispatch(&long_press);
  GetAppListTestHelper()->WaitUntilIdle();
  const aura::Window* root = window_util::GetRootWindowAt(onscreen_point);
  const RootWindowController* root_window_controller =
      RootWindowController::ForWindow(root);
  EXPECT_TRUE(root_window_controller->IsContextMenuShown());

  // Tap down to close the context menu.
  ui::GestureEvent tap_down(onscreen_point.x(), onscreen_point.y(), 0,
                            base::TimeTicks(),
                            ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));
  generator->Dispatch(&tap_down);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShown());

  // Right click to open the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->ClickRightButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_TRUE(root_window_controller->IsContextMenuShown());

  // Left click to close the context menu.
  generator->MoveMouseTo(onscreen_point);
  generator->ClickLeftButton();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_FALSE(root_window_controller->IsContextMenuShown());
}

// Tests app list visibility when switching to tablet mode during dragging from
// shelf.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       SwitchToTabletModeDuringDraggingFromShelf) {
  UpdateDisplay("1080x900");
  GetAppListTestHelper()->CheckVisibility(false);

  // Drag from the shelf to show the app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int x = 540;
  const int closed_y = 890;
  const int fullscreen_y = 0;
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list.
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Drag from the shelf to show the app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, fullscreen_y));
  GetAppListTestHelper()->CheckVisibility(true);

  // Switch to tablet mode.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to try to close app list.
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests app list visibility when switching to tablet mode during dragging to
// close app list.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       SwitchToTabletModeDuringDraggingToClose) {
  UpdateDisplay("1080x900");

  // Open app list.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const int x = 540;
  const int peeking_height =
      900 - AppListConfig::instance().peeking_app_list_height();
  const int closed_y = 890;
  generator->MoveTouch(gfx::Point(x, peeking_height));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);

  // Open app list.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);

  // Drag to shelf to close app list, meanwhile switch to tablet mode.
  generator->MoveTouch(gfx::Point(x, peeking_height));
  generator->PressTouch();
  generator->MoveTouch(gfx::Point(x, peeking_height + 10));
  EnableTabletMode(true);
  generator->MoveTouch(gfx::Point(x, closed_y));
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
}

// Test backdrop exists for active non-fullscreen window in tablet mode.
TEST_P(AppListPresenterDelegateHomeLauncherTest, BackdropTest) {
  WorkspaceControllerTestApi test_helper(ShellTestApi().workspace_controller());
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(test_helper.GetBackdropWindow());

  std::unique_ptr<aura::Window> non_fullscreen_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  non_fullscreen_window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_TRUE(test_helper.GetBackdropWindow());
}

// Tests that app list is not active when switching to tablet mode if an active
// window exists.
TEST_P(AppListPresenterDelegateHomeLauncherTest,
       NotActivateAppListWindowWhenActiveWindowExists) {
  // No window is active.
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Show app list in tablet mode. It should become active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(GetAppListView()->GetWidget()->GetNativeWindow(),
            window_util::GetActiveWindow());

  // End tablet mode.
  EnableTabletMode(false);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(nullptr, window_util::GetActiveWindow());

  // Activate a window.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  WindowState::Get(window.get())->Activate();
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Show app list in tablet mode. It should not be active.
  EnableTabletMode(true);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
}

// Tests that involve the virtual keyboard.
class AppListPresenterDelegateVirtualKeyboardTest
    : public AppListPresenterDelegateTest {
 public:
  AppListPresenterDelegateVirtualKeyboardTest() = default;
  ~AppListPresenterDelegateVirtualKeyboardTest() override = default;

  // AppListPresenterDelegateTest:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AppListPresenterDelegateTest::SetUp();
  }
};

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(,
                         AppListPresenterDelegateVirtualKeyboardTest,
                         testing::Bool());

// Tests that tapping or clicking the body of the applist with an active virtual
// keyboard results in the virtual keyboard closing with no side effects.
TEST_P(AppListPresenterDelegateVirtualKeyboardTest,
       TapAppListWithVirtualKeyboardDismissesVirtualKeyboard) {
  const bool test_click = GetParam();
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EnableTabletMode(true);

  // Tap to activate the searchbox.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureTapAt(GetPointInsideSearchbox());

  // Enter some text in the searchbox, the applist should transition to
  // fullscreen search.
  generator->PressKey(ui::KeyboardCode::VKEY_0, 0);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);

  // Manually show the virtual keyboard.
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::WaitUntilShown());

  // Tap or click outside the searchbox, the virtual keyboard should hide.
  if (test_click) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());

  // The searchbox should still be active and the AppListView should still be in
  // FULLSCREEN_SEARCH.
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  EXPECT_TRUE(GetAppListView()->search_box_view()->is_search_box_active());

  // Tap or click the body of the AppList again, the searchbox should deactivate
  // and the applist should be in FULLSCREEN_ALL_APPS.
  if (test_click) {
    generator->MoveMouseTo(GetPointOutsideSearchbox());
    generator->ClickLeftButton();
    generator->ReleaseLeftButton();
  } else {
    generator->GestureTapAt(GetPointOutsideSearchbox());
  }
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_FALSE(GetAppListView()->search_box_view()->is_search_box_active());
}

}  // namespace ash
