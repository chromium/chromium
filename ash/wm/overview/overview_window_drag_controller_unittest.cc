// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_window_drag_controller.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_util.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Drags the item by |x| and |y| and does not drop it.
void StartDraggingItemBy(OverviewItemBase* item,
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

}  // namespace

class OverviewWindowDragControllerTest : public AshTestBase {
 public:
  OverviewWindowDragControllerTest() = default;

  OverviewWindowDragControllerTest(const OverviewWindowDragControllerTest&) =
      delete;
  OverviewWindowDragControllerTest& operator=(
      const OverviewWindowDragControllerTest&) = delete;

  ~OverviewWindowDragControllerTest() override = default;

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  OverviewSession* overview_session() {
    return OverviewController::Get()->overview_session();
  }

  OverviewWindowDragController* drag_controller() {
    return overview_session()->window_drag_controller();
  }

  const SplitViewDragIndicators* drag_indicators() const {
    return OverviewController::Get()
        ->overview_session()
        ->grid_list()[0]
        ->split_view_drag_indicators();
  }

  OverviewGrid* overview_grid() {
    return overview_session()->GetGridWithRootWindow(
        Shell::GetPrimaryRootWindow());
  }

  const views::Widget* desks_bar_widget() {
    DCHECK(overview_grid()->desks_bar_view());
    return overview_grid()->desks_bar_view()->GetWidget();
  }

  OverviewItemBase* GetOverviewItemForWindow(aura::Window* window) {
    return overview_session()->GetOverviewItemForWindow(window);
  }

  int GetExpectedDesksBarShiftAmount() const {
    return drag_indicators()->GetLeftHighlightViewBounds().bottom() +
           kHighlightScreenEdgePaddingDp;
  }

  void StartDraggingAndValidateDesksBarShifted(aura::Window* window) {
    // Enter overview mode, and start dragging the window. Validate that the
    // desks bar widget is shifted down to make room for the indicators.
    EnterOverview();
    EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
    auto* overview_item = GetOverviewItemForWindow(window);
    ASSERT_TRUE(overview_item);
    StartDraggingItemBy(overview_item, 100, 200, /*by_touch_gestures=*/false,
                        GetEventGenerator());
    ASSERT_TRUE(drag_controller());
    EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNormalDrag,
              drag_controller()->current_drag_behavior_for_testing());
    ASSERT_TRUE(drag_indicators());
    EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
              drag_indicators()->current_window_dragging_state());
    // Note that it's ok to use screen bounds here since we only have a single
    // primary display.
    EXPECT_EQ(GetExpectedDesksBarShiftAmount(),
              desks_bar_widget()->GetWindowBoundsInScreen().y());
  }

  int GetDesksBarViewExpandedStateHeight(
      const OverviewDeskBarView* desks_bar_view) {
    return DeskBarViewBase::GetPreferredBarHeight(
        desks_bar_view->GetWidget()->GetNativeWindow()->GetRootWindow(),
        DeskBarViewBase::Type::kOverview, DeskBarViewBase::State::kExpanded);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kOverviewSessionInitOptimizations};
};

TEST_F(OverviewWindowDragControllerTest, NoDragToCloseUsingMouse) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  // Enter tablet mode and enter overview mode.
  // Avoid TabletModeController::OnGetSwitchStates() from disabling tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  auto* overview_controller = OverviewController::Get();
  EnterOverview();
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
            drag_controller->current_drag_behavior_for_testing());
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(target_bounds_before_drag, overview_item->target_bounds());
}

// Tests that the bounds of the drop target for `OverviewItem` will match that
// of the corresponding item which the drop target is a placeholder for.
TEST_F(OverviewWindowDragControllerTest, DropTargetBoundsTest) {
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desk_controller->desks().size());

  std::unique_ptr<aura::Window> window = CreateAppWindow();

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->item_list();
  ASSERT_EQ(window_list.size(), 1u);

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  auto* event_generator = GetEventGenerator();
  const gfx::RectF target_bounds_before_dragging =
      overview_item->target_bounds();

  for (const bool by_touch : {false, true}) {
    DragItemToPoint(
        overview_item,
        Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
        event_generator, by_touch, /*drop=*/false);
    EXPECT_TRUE(overview_controller->InOverviewSession());

    const OverviewItemBase* drop_target = overview_grid->drop_target();
    EXPECT_TRUE(drop_target);
    EXPECT_EQ(gfx::RectF(drop_target->item_widget()->GetWindowBoundsInScreen()),
              target_bounds_before_dragging);
    if (by_touch) {
      event_generator->ReleaseTouch();
    } else {
      event_generator->ReleaseLeftButton();
    }
  }
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
  EnterOverview();
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
            drag_controller->current_drag_behavior_for_testing());
  // Continue dragging vertically up such that the drag location intersects with
  // the desks bar. Expect that normal drag is now triggered.
  event_generator->MoveTouchBy(0, -(space_to_leave + 10));
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior_for_testing());
  // Now it's possible to drop it on desk_2's mini_view.
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
  ASSERT_TRUE(desk_2_mini_view);
  event_generator->MoveTouch(
      desk_2_mini_view->GetBoundsInScreen().CenterPoint());
  event_generator->ReleaseTouch();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_grid->empty());
  const Desk* desk_2 = controller->GetDeskAtIndex(1);
  EXPECT_TRUE(base::Contains(desk_2->windows(), window.get()));
  EXPECT_TRUE(const_cast<OverviewGrid*>(overview_grid)->no_windows_widget());
}

// Test that if window is destroyed during dragging, no crash should happen and
// drag should be reset.
TEST_F(OverviewWindowDragControllerTest, WindowDestroyedDuringDragging) {
  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);

  auto* event_generator = GetEventGenerator();
  StartDraggingItemBy(overview_item, 30, 200, /*by_touch_gestures=*/false,
                      event_generator);
  OverviewWindowDragController* drag_controller =
      overview_session->window_drag_controller();
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior_for_testing());

  window.reset();
  EXPECT_EQ(OverviewWindowDragController::DragBehavior::kNoDrag,
            drag_controller->current_drag_behavior_for_testing());
}

TEST_F(OverviewWindowDragControllerTest,
       DragAndDropWindowInPortraitModeWithOneDesk) {
  // Update the display to make it portrait mode.
  UpdateDisplay("768x1000");
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));

  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  StartDraggingAndValidateDesksBarShifted(window.get());
  const auto* desks_bar_view = overview_grid()->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Check the height of the desks bar view. Desks bar is transformed to
  // expanded state immediately at the beginning of the drag.
  EXPECT_EQ(GetDesksBarViewExpandedStateHeight(desks_bar_view),
            desks_bar_view->bounds().height());

  // Now drop `window`. Check the height of the desks bar view. It should still
  // be `kDeskBarZeroStateHeight`.
  auto* event_generator = GetEventGenerator();
  event_generator->ReleaseLeftButton();

  // Desks bar never goes back to zero state after it is initialized.
  EXPECT_EQ(GetDesksBarViewExpandedStateHeight(desks_bar_view),
            desks_bar_view->bounds().height());

  // Click on the zero state new desk button to create a new desk. This
  // shouldn't end overview mode. The desks bar view should be transformed to
  // the expanded state.
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  LeftClickOn(desks_bar_view->new_desk_button());
  EXPECT_EQ(GetDesksBarViewExpandedStateHeight(desks_bar_view),
            desks_bar_view->bounds().height());

  // Now remove the newly created desk. This shouldn't end overview mode. The
  // desks bar view should stay in expanded state.
  auto* controller = Shell::Get()->desks_controller();
  controller->RemoveDesk(controller->desks().back().get(),
                         DesksCreationRemovalSource::kButton,
                         DeskCloseType::kCombineDesks);
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(DeskBarViewBase::State::kExpanded, desks_bar_view->state());
  EXPECT_EQ(GetDesksBarViewExpandedStateHeight(desks_bar_view),
            desks_bar_view->bounds().height());
}

// Tests that dragging window in portrait mode won't cause overview items
// overlap with desks bar. Regression test for https://crbug.com/1275285.
TEST_F(OverviewWindowDragControllerTest, DragWindowInPortraitMode) {
  // Update the display to make it portrait mode.
  UpdateDisplay("768x1000");

  // Create 10 windows with size the same as the maximized window's size.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 10; ++i)
    windows.push_back(CreateAppWindow(gfx::Rect(0, 0, 768, 1269)));

  StartDraggingAndValidateDesksBarShifted(windows.back().get());
  const auto* desks_bar_view = overview_grid()->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  // Check there's no overlap between overview items and desks bar view. Since
  // the first overview item is still being dragged, we should use the second
  // item in the list to check if there's overlap or not.
  EXPECT_FALSE(
      desks_bar_view->GetBoundsInScreen().Intersects(gfx::ToEnclosedRect(
          overview_grid()->item_list()[1].get()->target_bounds())));
}

// Tests that if a user starts dragging an overview item and the desks bar(s)
// are in zero state, they will become expanded state.
TEST_F(OverviewWindowDragControllerTest, DesksBarState) {
  UpdateDisplay("800x600, 800x600");

  std::unique_ptr<aura::Window> window = CreateAppWindow();

  EnterOverview();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Since we only have one desk, the desk bars should be zero state currently.
  aura::Window::Windows roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  for (aura::Window* root : roots) {
    const auto* desks_bar_view = GetOverviewGridForRoot(root)->desks_bar_view();
    ASSERT_TRUE(desks_bar_view);
    ASSERT_EQ(DeskBarViewBase::State::kZero, desks_bar_view->state());
  }

  // Start dragging the item, but not enough to snap or move to another desk.
  auto* overview_item = GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);
  StartDraggingItemBy(overview_item, /*x=*/5, /*y=*/5,
                      /*by_touch_gestures=*/false, GetEventGenerator());

  // All the desks bar should be expanded.
  for (aura::Window* root : roots) {
    const auto* desks_bar_view = GetOverviewGridForRoot(root)->desks_bar_view();
    ASSERT_TRUE(desks_bar_view);
    EXPECT_EQ(DeskBarViewBase::State::kExpanded, desks_bar_view->state());
  }
}

// Tests the behavior of dragging a window in portrait tablet mode with virtual
// desks enabled.
class OverviewWindowDragControllerDesksPortraitTabletTest
    : public OverviewWindowDragControllerTest {
 public:
  OverviewWindowDragControllerDesksPortraitTabletTest() = default;

  OverviewWindowDragControllerDesksPortraitTabletTest(
      const OverviewWindowDragControllerDesksPortraitTabletTest&) = delete;
  OverviewWindowDragControllerDesksPortraitTabletTest& operator=(
      const OverviewWindowDragControllerDesksPortraitTabletTest&) = delete;

  ~OverviewWindowDragControllerDesksPortraitTabletTest() override = default;

  // OverviewWindowDragControllerTest:
  void SetUp() override {
    OverviewWindowDragControllerTest::SetUp();

    // Setup a portrait internal display in tablet mode.
    UpdateDisplay("800x700");
    const int64_t display_id =
        display::Screen::GetScreen()->GetPrimaryDisplay().id();
    set_internal_ = std::make_unique<display::test::ScopedSetInternalDisplayId>(
        display_manager(), display_id);
    ScreenOrientationControllerTestApi test_api(
        Shell::Get()->screen_orientation_controller());
    // Set the screen orientation to primary portrait.
    test_api.SetDisplayRotation(display::Display::ROTATE_270,
                                display::Display::RotationSource::ACTIVE);
    EXPECT_EQ(test_api.GetCurrentOrientation(),
              chromeos::OrientationType::kPortraitPrimary);
    // Enter tablet mode. Avoid TabletModeController::OnGetSwitchStates() from
    // disabling tablet mode.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

    // Setup two desks.
    auto* desks_controller = DesksController::Get();
    desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
    ASSERT_EQ(2u, desks_controller->desks().size());

    // Give the second desk a name. The desk name gets exposed as the accessible
    // name. And the focusable views that are painted in these tests will fail
    // the accessibility paint checker checks if they lack an accessible name.
    desks_controller->GetDeskAtIndex(1)->SetName(u"Desk 2", false);
  }

 private:
  std::unique_ptr<display::test::ScopedSetInternalDisplayId> set_internal_;
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
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
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
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary,
            drag_indicators()->current_window_dragging_state());
  EXPECT_EQ(OverviewGridTestApi(overview_grid()).bounds().y(),
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
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            drag_indicators()->current_window_dragging_state());
  EXPECT_EQ(OverviewGridTestApi(overview_grid()).bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());

  // Drop it at this location and expect the window to snap. The desks bar
  // remains unshifted.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(window.get(), split_view_controller()->primary_window());
  EXPECT_EQ(OverviewGridTestApi(overview_grid()).bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());
}

TEST_F(OverviewWindowDragControllerDesksPortraitTabletTest, DragAndDropInDesk) {
  auto window = CreateAppWindow(gfx::Rect(0, 0, 250, 100));
  StartDraggingAndValidateDesksBarShifted(window.get());

  // Drag the window to the second desk's mini_view. While dragging is in
  // progress, the desks bar remains shifted.
  const auto* desks_bar_view = overview_grid()->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  auto* desk_2_mini_view = desks_bar_view->mini_views()[1].get();
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
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(OverviewGridTestApi(overview_grid()).bounds().y(),
            desks_bar_widget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            drag_indicators()->current_window_dragging_state());
}

// Tests that dragging window in tablet portrait mode won't cause overview items
// overlap with desks bar. Regression test for https://crbug.com/1275285.
TEST_F(OverviewWindowDragControllerDesksPortraitTabletTest,
       DragWindowInPortraitMode) {
  UpdateDisplay("700x1000");

  // Create 9 windows to make sure we can use tablet mode grid layout.
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 9; ++i) {
    windows.push_back(CreateAppWindow());
  }

  StartDraggingAndValidateDesksBarShifted(windows[4].get());

  // Delete desk2 in the overview mode. Note if we delete desk2 outside of the
  // overview mode, there's no desks bar after entering overview mode. Cause we
  // don't show desks bar for tablet mode when there's only one desk.
  auto* desks_controller = DesksController::Get();
  DesksController::Get()->RemoveDesk(desks_controller->GetDeskAtIndex(1),
                                     DesksCreationRemovalSource::kButton,
                                     DeskCloseType::kCombineDesks);
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Check desks bar still exists after desk2 gets removed.
  const auto* desks_bar_view = overview_grid()->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  const gfx::Rect desk_bar_bounds = desks_bar_view->GetBoundsInScreen();
  const gfx::Rect first_item_bounds =
      gfx::ToEnclosedRect(overview_grid()->item_list()[0]->target_bounds());
  if (features::IsForestFeatureEnabled()) {
    // With forest, a little overlap is ok since the desk bar is transparent.
    // TODO(sammiequon|zxdan): Check if this gap is okay.
    EXPECT_NEAR(desk_bar_bounds.bottom(), first_item_bounds.y(), 20);
  } else {
    // Check there's no overlap between overview items and desks bar view.
    EXPECT_FALSE(desk_bar_bounds.Intersects(first_item_bounds));
  }
}

}  // namespace ash
