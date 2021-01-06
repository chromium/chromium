// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_drag_controller.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

using ash::desks_util::BelongsToActiveDesk;

namespace ash {

namespace {

// Drags the item by |x| and |y| and does not drop it.
void StartDraggingItemBy(OverviewItem* item,
                         int x,
                         int y,
                         bool by_touch_gestures,
                         ui::test::EventGenerator* event_generator) {
  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  event_generator->set_current_screen_location(item_center);
  if (by_touch_gestures) {
    event_generator->PressTouch();
    event_generator->MoveTouchBy(x, y);
  } else {
    event_generator->PressLeftButton();
    event_generator->MoveMouseBy(x, y);
  }
}

// Given x, and y of a point, returns the screen in pixels corresponding point
// which can be used to provide event locations to the event generator when
// the display is rotated.
gfx::Point GetScreenInPixelsPoint(int x, int y) {
  gfx::Point point{x, y};
  Shell::GetPrimaryRootWindow()->GetHost()->ConvertDIPToScreenInPixels(&point);
  return point;
}

// Waits for a window to be destroyed.
class WindowCloseWaiter : public aura::WindowObserver {
 public:
  explicit WindowCloseWaiter(aura::Window* window) : window_(window) {
    DCHECK(window_);
    window_->AddObserver(this);
  }

  ~WindowCloseWaiter() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  void Wait() {
    // Did window close already?
    if (!window_)
      return;

    run_loop_.Run();
  }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    window_ = nullptr;
    run_loop_.Quit();
  }

 private:
  aura::Window* window_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowCloseWaiter);
};

}  // namespace

using OverviewWindowDragControllerTest = AshTestBase;

TEST_F(OverviewWindowDragControllerTest, NoDragToCloseUsingMouse) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and enter overview mode.
  // Avoid TabletModeController::OnGetSwitchStates() from disabling tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();
  auto* event_generator = GetEventGenerator();

  // Drag with mouse by a bigger Y-component than X, which would normally
  // trigger the drag-to-close mode, but won't since this mode only work with
  // touch gestures.
  StartDraggingItemBy(overview_item, 30, 200, /*by_touch_gestures=*/false,
                      event_generator);
  OverviewWindowDragController* drag_controller =
      overview_session->window_drag_controller();
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior());
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
}

TEST_F(OverviewWindowDragControllerTest,
       SwitchDragToCloseToNormalDragWhenDraggedToDesk) {
  UpdateDisplay("600x800");
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, controller->desks().size());

  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  auto* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  const auto* overview_grid =
      overview_session->GetGridWithRootWindow(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  const gfx::RectF target_bounds_before_drag = overview_item->target_bounds();

  // Drag with touch gesture only vertically without intersecting with the desk
  // bar, which should trigger the drag-to-close mode.
  const int item_center_to_desks_bar_bottom =
      gfx::ToRoundedPoint(target_bounds_before_drag.CenterPoint()).y() -
      desks_bar_view->GetBoundsInScreen().bottom();
  EXPECT_GT(item_center_to_desks_bar_bottom, 0);
  const int space_to_leave = 20;
  auto* event_generator = GetEventGenerator();
  StartDraggingItemBy(overview_item, 0,
                      -(item_center_to_desks_bar_bottom - space_to_leave),
                      /*by_touch_gestures=*/true, event_generator);
  OverviewWindowDragController* drag_controller =
      overview_session->window_drag_controller();
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kDragToClose,
            drag_controller->current_drag_behavior());
  // Continue dragging vertically up such that the drag location intersects with
  // the desks bar. Expect that normal drag is now triggered.
  event_generator->MoveTouchBy(0, -(space_to_leave + 10));
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior());
  // Now it's possible to drop it on desk_2's mini_view.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  ASSERT_TRUE(desk_2_mini_view);
  event_generator->MoveTouch(
      desk_2_mini_view->GetBoundsInScreen().CenterPoint());
  event_generator->ReleaseTouch();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  const Desk* desk_2 = controller->desks()[1].get();
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_TRUE(overview_session->no_windows_widget_for_testing());
}

// Tests the behavior of dragging a window in portrait tablet mode with virtual
// desks enabled.
class OverviewWindowDragControllerDesksPortraitTabletTest : public AshTestBase {
 public:
  OverviewWindowDragControllerDesksPortraitTabletTest() = default;
  ~OverviewWindowDragControllerDesksPortraitTabletTest() override = default;

  OverviewController* overview_controller() {
    return Shell::Get()->overview_controller();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  OverviewSession* overview_session() {
    DCHECK(overview_controller()->InOverviewSession());
    return overview_controller()->overview_session();
  }

  OverviewWindowDragController* drag_controller() {
    return overview_session()->window_drag_controller();
  }

  SplitViewDragIndicators* drag_indicators() {
    return overview_session()->grid_list()[0]->split_view_drag_indicators();
  }

  OverviewGrid* overview_grid() {
    return overview_session()->GetGridWithRootWindow(
        Shell::GetPrimaryRootWindow());
  }

  const views::Widget* desks_bar_widget() {
    DCHECK(overview_grid()->desks_bar_view());
    return overview_grid()->desks_bar_view()->GetWidget();
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Setup a portrait internal display in tablet mode.
    UpdateDisplay("800x600");
    const int64_t display_id =
        display::Screen::GetScreen()->GetPrimaryDisplay().id();
    display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                           display_id);
    ScreenOrientationControllerTestApi test_api(
        Shell::Get()->screen_orientation_controller());
    // Set the screen orientation to primary portrait.
    test_api.SetDisplayRotation(display::Display::ROTATE_270,
                                display::Display::RotationSource::ACTIVE);
    EXPECT_EQ(test_api.GetCurrentOrientation(),
              OrientationLockType::kPortraitPrimary);
    // Enter tablet mode. Avoid TabletModeController::OnGetSwitchStates() from
    // disabling tablet mode.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

    // Setup two desks.
    auto* desks_controller = DesksController::Get();
    desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, desks_controller->desks().size());
  }

  OverviewItem* GetOverviewItemForWindow(aura::Window* window) {
    return overview_session()->GetOverviewItemForWindow(window);
  }

  int GetExpectedDesksBarShiftAmount() {
    return drag_indicators()->GetLeftHighlightViewBounds().bottom() +
           kHighlightScreenEdgePaddingDp;
  }

  void StartDraggingAndValidateDesksBarShifted(aura::Window* window) {
    // Enter overview mode, and start dragging the window. Validate that the
    // desks bar widget is shifted down to make room for the indicators.
    overview_controller()->StartOverview();
    EXPECT_TRUE(overview_controller()->InOverviewSession());
    auto* overview_item = GetOverviewItemForWindow(window);
    ASSERT_TRUE(overview_item);
    StartDraggingItemBy(overview_item, 30, 200, /*by_touch_gestures=*/false,
                        GetEventGenerator());
    ASSERT_TRUE(drag_controller());
    EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNormalDrag,
              drag_controller()->current_drag_behavior());
    ASSERT_TRUE(drag_indicators());
    EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
              drag_indicators()->current_window_dragging_state());
    // Note that it's ok to use screen bounds here since we only have a single
    // primary display.
    EXPECT_EQ(GetExpectedDesksBarShiftAmount(),
              desks_bar_widget()->GetWindowBoundsInScreen().y());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewWindowDragControllerDesksPortraitTabletTest);
};

TEST_F(OverviewWindowDragControllerDesksPortraitTabletTest,
       DragAndDropInEmptyArea) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  StartDraggingAndValidateDesksBarShifted(window.get());

  // Dropping the window any where outside the bounds of the desks widget or the
  // snap bounds should restore the desks widget to its correct position.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(GetScreenInPixelsPoint(300, 400));
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(0, desks_bar_widget()->GetWindowBoundsInScreen().y());
}

TEST_F(OverviewWindowDragControllerDesksPortraitTabletTest,
       DragAndDropInSnapAreas) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  StartDraggingAndValidateDesksBarShifted(window.get());

  // Drag towards the area at the bottom of the display and note that the desks
  // bar widget is not shifted.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(GetScreenInPixelsPoint(300, 800));
  ASSERT_TRUE(drag_indicators());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            drag_indicators()->current_window_dragging_state());
  EXPECT_EQ(overview_grid()->bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());

  // Drag back to the middle, the desks bar should be shifted again.
  event_generator->MoveMouseTo(GetScreenInPixelsPoint(300, 400));
  ASSERT_TRUE(drag_indicators());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            drag_indicators()->current_window_dragging_state());
  EXPECT_EQ(GetExpectedDesksBarShiftAmount(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());

  // Drag towards the area at the top of the display and note that the desks bar
  // widget is no longer shifted.
  event_generator->MoveMouseTo(GetScreenInPixelsPoint(300, 0));
  ASSERT_TRUE(drag_indicators());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            drag_indicators()->current_window_dragging_state());
  EXPECT_EQ(overview_grid()->bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());

  // Drop it at this location and expect the window to snap. The desks bar
  // remains unshifted.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->left_window());
  EXPECT_EQ(overview_grid()->bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());
}

TEST_F(OverviewWindowDragControllerDesksPortraitTabletTest, DragAndDropInDesk) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  StartDraggingAndValidateDesksBarShifted(window.get());

  // Drag the window to the second desk's mini_view. While dragging is in
  // progress, the desks bar remains shifted.
  const auto* desks_bar_view = overview_grid()->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1];
  ASSERT_TRUE(desk_2_mini_view);
  const auto mini_view_location =
      desk_2_mini_view->GetBoundsInScreen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      GetScreenInPixelsPoint(mini_view_location.x(), mini_view_location.y()));
  ASSERT_TRUE(drag_indicators());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            drag_indicators()->current_window_dragging_state());
  EXPECT_EQ(GetExpectedDesksBarShiftAmount(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());

  // Once the window is dropped on that desk, the desks bar should return to its
  // unshifted position, and the window should move to the second desk.
  EXPECT_TRUE(desks_util::BelongsToActiveDesk(window.get()));
  event_generator->ReleaseLeftButton();  // Drop.
  EXPECT_FALSE(desks_util::BelongsToActiveDesk(window.get()));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(overview_grid()->bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            drag_indicators()->current_window_dragging_state());
}

}  // namespace ash
