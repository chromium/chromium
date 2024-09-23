// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/drag_window_from_shelf_controller.h"
#include "ash/shelf/drag_window_from_shelf_controller_test_api.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}

}  // namespace

// Test base for unit test related to drag handle contextual nudges.
class DragHandleContextualNudgeTest : public ShelfLayoutManagerTestBase {
 public:
  DragHandleContextualNudgeTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kHideShelfControlsInTabletMode);
  }
  ~DragHandleContextualNudgeTest() override = default;

  DragHandleContextualNudgeTest(const DragHandleContextualNudgeTest& other) =
      delete;
  DragHandleContextualNudgeTest& operator=(
      const DragHandleContextualNudgeTest& other) = delete;

  // ShelfLayoutManagerTestBase:
  void SetUp() override {
    ShelfLayoutManagerTestBase::SetUp();
    test_clock_.Advance(base::Hours(2));
    contextual_tooltip::OverrideClockForTesting(&test_clock_);
  }
  void TearDown() override {
    contextual_tooltip::ClearClockOverrideForTesting();
    AshTestBase::TearDown();
  }

  base::SimpleTestClock test_clock_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DragHandleContextualNudgeTestA11yPrefs
    : public DragHandleContextualNudgeTest,
      public ::testing::WithParamInterface<std::string> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    DragHandleContextualNudgeTestA11yPrefs,
    testing::Values(prefs::kAccessibilityAutoclickEnabled,
                    prefs::kAccessibilitySpokenFeedbackEnabled,
                    prefs::kAccessibilitySwitchAccessEnabled));

TEST_F(DragHandleContextualNudgeTest, ShowDragHandleNudgeWithTimer) {
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  // The drag handle should be showing but the nudge should not. A timer to show
  // the nudge should be initialized.
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  // Firing the timer should show the drag handle nudge.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
}

TEST_F(DragHandleContextualNudgeTest, HideDragHandleNudgeHiddenOnMinimize) {
  base::HistogramTester histogram_tester;

  // Creates a test window to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  // The drag handle and nudge should be showing after the timer fires.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Minimizing the widget should hide the drag handle and nudge.
  widget->Minimize();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
}

// Tests that the drag handle nudge nudge is hidden when closing the widget and
// setting the ShelfBackgroundType to kHomeLauncher.
TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeHiddenOnClose) {
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  DragHandle* const drag_handle = GetShelfWidget()->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  // Close the widget.
  widget->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  EXPECT_FALSE(drag_handle->GetVisible());
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
}

// Checks that the shelf cannot be auto hidden while animating shelf drag handle
// nudge.
TEST_F(DragHandleContextualNudgeTest,
       HideDragHandleDoesNotInteruptShowNudgeAnimation) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  ui::ScopedAnimationDurationScaleMode normal_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  GetShelfWidget()->GetDragHandle()->MaybeShowDragHandleNudge();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->show_nudge_animation_in_progress());
  GetShelfLayoutManager()->UpdateAutoHideState();
  // Shelf auto hide should not interrupt animations to show drag handle nudge.
  // Showing the nudge while hiding the shelf is not intended behavior.
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->show_nudge_animation_in_progress());
}

// Checks that the drag handle nudge is not shown when entering kInApp with
// shelf autohide turned on.
TEST_F(DragHandleContextualNudgeTest, DragHandleNotShownForAutoHideShelf) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->show_nudge_animation_in_progress());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
}

TEST_F(DragHandleContextualNudgeTest, DoNotShowNudgeWithoutDragHandle) {
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  // Minimizing the widget should hide the drag handle and nudge.
  widget->Minimize();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
}

TEST_F(DragHandleContextualNudgeTest,
       ContinueShowingDragHandleNudgeOnActiveWidgetChanged) {
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Maximizing and showing a different widget should not hide the drag handle
  // or nudge.
  views::Widget* new_widget = CreateTestWidget();
  new_widget->Maximize();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeShownInAppShelf) {
  base::HistogramTester histogram_tester;

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // Drag handle and nudge should not be shown in clamshell mode.
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Test that the first time a user transitions into tablet mode with a
  // maximized window will show the drag nudge immedietly. The drag handle nudge
  // should not be visible yet and the timer to show it should be set.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_show_drag_handle_timer_for_testing());

  // Firing the timer should show the nudge for the first time. The nudge should
  // remain visible until the shelf state changes so the timer to hide it should
  // not be set.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_hide_drag_handle_timer_for_testing());

  // Leaving tablet mode should hide the nudge.
  TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Reentering tablet mode should show the drag handle but the nudge should
  // not. No timer should be set to show the nudge.
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());

  // Advance time for more than a day (which should enable the nudge again).
  test_clock_.Advance(base::Hours(25));

  // Reentering tablet mode with a maximized widget should immedietly show the
  // drag handle and set a timer to show the nudge.
  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  // Firing the timer should show the nudge.
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_show_drag_handle_timer_for_testing());
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());
  // On subsequent shows, the nudge should be hidden after a timeout.
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_hide_drag_handle_timer_for_testing());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeShownOnTap) {
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_show_drag_handle_timer_for_testing());
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Exiting and re-entering tablet should hide the nudge and put the shelf into
  // the default kInApp shelf state.
  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Tapping the drag handle should show the drag handle nudge immedietly and
  // the show nudge timer should be set.
  GetEventGenerator()->GestureTapAt(
      GetShelfWidget()->GetDragHandle()->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_TRUE(GetShelfWidget()
                  ->GetDragHandle()
                  ->has_hide_drag_handle_timer_for_testing());
}

TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeNotShownForHiddenShelf) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  // The shelf is hidden, so the drag handle nudge should not be shown.
  EXPECT_TRUE(drag_handle->GetVisible());
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
  EXPECT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());

  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Back gesture nudge should be allowed if the shelf is hidden.
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      prefs, contextual_tooltip::TooltipType::kBackGesture, nullptr));

  // Swipe up to show the shelf - this should schedule the drag handle nudge.
  SwipeUpOnShelf();

  // Back gesture nudge should be disallowed at this time, given that the drag
  // handle nudge can be shown.
  EXPECT_FALSE(contextual_tooltip::ShouldShowNudge(
      prefs, contextual_tooltip::TooltipType::kBackGesture, nullptr));

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());
}

// Tapping the drag handle nudge when auto hide shelf is enabled should hide the
// drag handle nudge but should not hide the shelf or hotseat.
TEST_F(DragHandleContextualNudgeTest,
       DragHandleNudgeTapDoesNotHideAutoHiddenShelf) {
  // Sets shelf auto hide behavior.
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  // The shelf and drag handle should be hidden and the nudge should not be
  // scheduled because shelf auto hide is set.
  EXPECT_TRUE(GetPrimaryShelf()->GetAutoHideState() ==
              ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN);
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
  EXPECT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());

  // Swipe up to show the shelf. This should show the shelf, extend the hotseat,
  // and schedule the drag handle nudge.
  SwipeUpOnShelf();
  EXPECT_TRUE(GetPrimaryShelf()->GetAutoHideState() ==
              ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  // Firing the show timer should create the nudge set the target visibility for
  // animations..
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());
  EXPECT_TRUE(drag_handle->drag_handle_nudge() != nullptr);

  // Tapping the drag handle nudge should hide the nudge but not affect the
  // visibility of the shelf or hotseat.
  GetEventGenerator()->GestureTapAt(
      drag_handle->drag_handle_nudge()->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
  EXPECT_TRUE(GetPrimaryShelf()->GetAutoHideState() ==
              ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Swiping down on shelf should hide the shelf and hotseat.
  SwipeDownOnShelf();
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  EXPECT_TRUE(GetPrimaryShelf()->GetAutoHideState() ==
              ShelfAutoHideState::SHELF_AUTO_HIDE_HIDDEN);

  // Swiping up on shelf should show the shelf and drag handle but not the
  // nudge or hotseat.
  SwipeUpOnShelf();
  EXPECT_TRUE(GetPrimaryShelf()->GetAutoHideState() ==
              ShelfAutoHideState::SHELF_AUTO_HIDE_SHOWN);
  EXPECT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());
  EXPECT_TRUE(drag_handle->drag_handle_nudge() == nullptr);
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Tests that drag handle show is canceled when the shelf is hidden while the
// drag handle is scheduled to be shown.
TEST_F(DragHandleContextualNudgeTest, HidingShelfCancelsDragHandleShow) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  // The shelf is hidden, so the drag handle nudge should not be shown.
  EXPECT_TRUE(drag_handle->GetVisible());
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
  EXPECT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());

  // Swipe up to show the shelf - this should schedule the drag handle nudge.
  SwipeUpOnShelf();

  EXPECT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());

  // Hide the shelf, and verify the drag handle show is canceled.
  SwipeDownOnShelf();
  EXPECT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());

  PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Back gesture nudge should be allowed if the shelf is hidden.
  EXPECT_TRUE(contextual_tooltip::ShouldShowNudge(
      prefs, contextual_tooltip::TooltipType::kBackGesture, nullptr));
}

// Tests that the drag handle nudge is not hidden when the user extends the
// hotseat.
TEST_F(DragHandleContextualNudgeTest,
       DragHandleNudgeNotHiddenByExtendingHotseat) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  // Swipe up to extend the hotseat - verify that the drag handle remain
  // visible.
  SwipeUpOnShelf();
  EXPECT_TRUE(drag_handle->GetVisible());
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());
}

// Tests that the drag handle nudge is horizontally centered in screen, and
// drawn above the shelf drag handle, even after display bounds are updated.
TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeBoundsInScreen) {
  UpdateDisplay("675x1200");
  TabletModeControllerTestApi().EnterTabletMode();

  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  EXPECT_TRUE(drag_handle->GetVisible());
  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  // Calculates absolute difference between horizontal margins of |inner| rect
  // within |outer| rect.
  auto margin_diff = [](const gfx::Rect& inner, const gfx::Rect& outer) -> int {
    const int left = inner.x() - outer.x();
    EXPECT_GE(left, 0);

    const int right = outer.right() - inner.right();
    EXPECT_GE(right, 0);

    return std::abs(left - right);
  };

  // Verify that nudge widget is centered in shelf.
  gfx::Rect shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  gfx::Rect nudge_bounds =
      drag_handle->drag_handle_nudge()->label()->GetBoundsInScreen();
  EXPECT_LE(margin_diff(nudge_bounds, shelf_bounds), 1);

  // Verify that the nudge vertical bounds - within the shelf bounds, and above
  // the drag handle.
  gfx::Rect drag_handle_bounds = drag_handle->GetBoundsInScreen();
  EXPECT_LE(shelf_bounds.y(), nudge_bounds.y());
  EXPECT_LE(nudge_bounds.bottom(), drag_handle_bounds.y());

  // Change the display bounds, and verify the updated drag handle bounds.
  UpdateDisplay("1200x675");
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Verify that nudge widget is centered in shelf.
  shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  nudge_bounds = drag_handle->drag_handle_nudge()->label()->GetBoundsInScreen();
  EXPECT_LE(margin_diff(nudge_bounds, shelf_bounds), 1);

  // Verify that the nudge vertical bounds - within the shelf bounds, and above
  // the drag handle.
  drag_handle_bounds = drag_handle->GetBoundsInScreen();
  EXPECT_LE(shelf_bounds.y(), nudge_bounds.y());
  EXPECT_LE(nudge_bounds.bottom(), drag_handle_bounds.y());
}

// Tests that drag handle does not hide during the window drag from shelf
// gesture.
TEST_F(DragHandleContextualNudgeTest,
       DragHandleNudgeNotHiddenDuringWindowDragFromShelf) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  TabletModeControllerTestApi().LeaveTabletMode();
  // Advance time for more than a day (which should enable the nudge again).
  test_clock_.Advance(base::Hours(25));
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->has_hide_drag_handle_timer_for_testing());

  const gfx::Point start = drag_handle->GetBoundsInScreen().CenterPoint();
  // Simulates window drag from shelf gesture, and verifies that the timer to
  // hide the drag handle nudge is canceled when the window drag from shelf
  // starts.
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      start, start + gfx::Vector2d(0, -200), base::Milliseconds(50),
      /*num_steps = */ 6,
      base::BindRepeating(
          [](DragHandle* drag_handle, ui::EventType type,
             const gfx::Vector2dF& offset) {
            DragWindowFromShelfController* window_drag_controller =
                GetShelfLayoutManager()->window_drag_controller_for_testing();
            if (window_drag_controller &&
                window_drag_controller->dragged_window()) {
              EXPECT_FALSE(
                  drag_handle->has_hide_drag_handle_timer_for_testing());
            }
            const bool scroll_end = type == ui::EventType::kGestureScrollEnd;
            EXPECT_EQ(!scroll_end,
                      drag_handle->gesture_nudge_target_visibility());
          },
          drag_handle));

  // The nudge should be hidden when the gesture completes.
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
}

// Tests that window drag from shelf cancels drag handle contextual nudge from
// showing.
TEST_F(DragHandleContextualNudgeTest,
       DragHandleNudgeNotShownDuringWindowDragFromShelf) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  EXPECT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());

  const gfx::Point start =
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
  // Simulates window drag from shelf gesture, and verifies that the timer to
  // show the drag handle nudge is canceled when the window drag from shelf
  // starts.
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      start, start + gfx::Vector2d(0, -200), base::Milliseconds(50),
      /*num_steps = */ 6,
      base::BindRepeating(
          [](DragHandle* drag_handle, ui::EventType type,
             const gfx::Vector2dF& offset) {
            DragWindowFromShelfController* window_drag_controller =
                GetShelfLayoutManager()->window_drag_controller_for_testing();
            if (window_drag_controller &&
                window_drag_controller->dragged_window()) {
              EXPECT_FALSE(
                  drag_handle->has_show_drag_handle_timer_for_testing());

              // Attempt to schedule the nudge should fail.
              if (type != ui::EventType::kGestureScrollEnd) {
                drag_handle->ScheduleShowDragHandleNudge();
                EXPECT_FALSE(
                    drag_handle->has_show_drag_handle_timer_for_testing());
              }
            }
            EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
          },
          drag_handle));

  // The nudge should be hidden when the gesture completes.
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
}

TEST_F(DragHandleContextualNudgeTest, GestureSwipeHidesDragHandleNudge) {
  base::HistogramTester histogram_tester;

  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  const gfx::Point start = drag_handle->GetBoundsInScreen().CenterPoint();
  // Simulates a swipe up from the drag handle to perform the in app to home
  // gesture.
  GetEventGenerator()->GestureScrollSequence(
      start, start + gfx::Vector2d(0, -300), base::Milliseconds(10),
      /*num_steps = */ 5);

  // The nudge should be hidden when the gesture completes.
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that drag handle nudge gets hidden when the user performs window drag
// from shelf to home.
TEST_F(DragHandleContextualNudgeTest, FlingFromShelfToHomeHidesTheNudge) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  const gfx::Point start = drag_handle->GetBoundsInScreen().CenterPoint();
  // Simulates window drag from shelf gesture, and verifies that the timer to
  // hide the drag handle nudge is canceled when the window drag from shelf
  // starts.
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      start, start + gfx::Vector2d(0, -300), base::Milliseconds(10),
      /*num_steps = */ 6,
      base::BindRepeating(
          [](DragHandle* drag_handle, ui::EventType type,
             const gfx::Vector2dF& offset) {
            const bool scroll_end = type == ui::EventType::kGestureScrollEnd;
            EXPECT_EQ(!scroll_end,
                      drag_handle->gesture_nudge_target_visibility());
          },
          drag_handle));

  // The nudge should be hidden when the gesture completes.
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
  GetAppListTestHelper()->CheckVisibility(true);
}

// Tests that drag handle nudge gets hidden when the user performs window drag
// from shelf, even if the gesture does not end up going home.
TEST_F(DragHandleContextualNudgeTest, DragFromShelfToHomeHidesTheNudge) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  const gfx::Point start = drag_handle->GetBoundsInScreen().CenterPoint();
  // Simulates window drag from shelf gesture, and verifies that the timer to
  // hide the drag handle nudge is canceled when the window drag from shelf
  // starts.
  GetEventGenerator()->GestureScrollSequenceWithCallback(
      start, start + gfx::Vector2d(0, -150), base::Milliseconds(500),
      /*num_steps = */ 20,
      base::BindRepeating(
          [](DragHandle* drag_handle, ui::EventType type,
             const gfx::Vector2dF& offset) {
            DragWindowFromShelfController* window_drag_controller =
                GetShelfLayoutManager()->window_drag_controller_for_testing();
            if (window_drag_controller &&
                window_drag_controller->dragged_window()) {
              DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
                  window_drag_controller);
            }
            const bool scroll_end = type == ui::EventType::kGestureScrollEnd;
            EXPECT_EQ(!scroll_end,
                      drag_handle->gesture_nudge_target_visibility());
          },
          drag_handle));

  // The nudge should be hidden when the gesture completes.
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());

  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that scheduled drag handle nudge show is canceled when overview starts.
TEST_F(DragHandleContextualNudgeTest, OverviewCancelsNudgeShow) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  EnterOverview();
  ASSERT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());
}

// Tests that tapping the drag handle can shown drag handle nudge in overview.
TEST_F(DragHandleContextualNudgeTest, DragHandleTapShowNudgeInOverview) {
  TabletModeControllerTestApi().EnterTabletMode();

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  ASSERT_TRUE(drag_handle->has_show_drag_handle_timer_for_testing());
  drag_handle->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  TabletModeControllerTestApi().LeaveTabletMode();
  TabletModeControllerTestApi().EnterTabletMode();

  EnterOverview();
  ASSERT_FALSE(drag_handle->has_show_drag_handle_timer_for_testing());

  GetEventGenerator()->GestureTapAt(
      drag_handle->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(drag_handle->GetVisible());
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  EXPECT_TRUE(drag_handle->has_hide_drag_handle_timer_for_testing());
  drag_handle->fire_hide_drag_handle_timer_for_testing();
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());

  // Tapping the drag handle again will show the nudge again.
  GetEventGenerator()->GestureTapAt(
      drag_handle->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());
}

// Tests that tapping the drag handle in split screen does not show nudge.
TEST_F(DragHandleContextualNudgeTest,
       DragHandleTapDoesNotShowNudgeForSplitScreen) {
  TabletModeControllerTestApi().EnterTabletMode();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  // Go into split view mode by first going into overview, and then snapping
  // the open window on one side.
  EnterOverview();
  SplitViewController* split_view_controller =
      SplitViewController::Get(shelf_widget->GetNativeWindow());
  split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Tapping the drag handle will not show the drag handle.
  GetEventGenerator()->GestureTapAt(
      drag_handle->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
}

// Tests that entering split screen hides the drag handle nudge.
TEST_F(DragHandleContextualNudgeTest, DragHandleNudgeHiddenOnSplitScreen) {
  TabletModeControllerTestApi().EnterTabletMode();

  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  ShelfWidget* const shelf_widget = GetShelfWidget();
  DragHandle* const drag_handle = shelf_widget->GetDragHandle();

  // Tapping the drag handle should show the drag handle.
  GetEventGenerator()->GestureTapAt(
      drag_handle->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(drag_handle->gesture_nudge_target_visibility());

  // Go into split view mode by first going into overview, and then snapping
  // the open window on one side.
  EnterOverview();
  SplitViewController* split_view_controller =
      SplitViewController::Get(shelf_widget->GetNativeWindow());
  split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // The drag handle nudge should no longer be visible.
  EXPECT_FALSE(drag_handle->gesture_nudge_target_visibility());
}

TEST_P(DragHandleContextualNudgeTestA11yPrefs, HideNudgesForShelfControls) {
  SCOPED_TRACE(testing::Message() << "Pref=" << GetParam());
  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  // The drag handle should be showing but the nudge should not. A timer to show
  // the nudge should be initialized.
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  // Firing the timer should show the drag handle nudge.
  GetShelfWidget()->GetDragHandle()->fire_show_drag_handle_timer_for_testing();
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_TRUE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());

  // Enabling accessibility auto click disables gestures and enables shelf
  // control buttons. In app to home nudge should be hidden.
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(GetParam(), true);
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
}

TEST_P(DragHandleContextualNudgeTestA11yPrefs, DisableNudgesForShelfControls) {
  SCOPED_TRACE(testing::Message() << "Pref=" << GetParam());
  // Turn on accessibility settings to enable shelf controls.
  Shell::Get()
      ->session_controller()
      ->GetLastActiveUserPrefService()
      ->SetBoolean(GetParam(), true);

  // Creates a widget to put shelf into in-app state.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());
  // The drag handle should be showing but the nudge should not. A timer to show
  // the nudge should not be initialized because shelf controls are on.
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  EXPECT_FALSE(
      GetShelfWidget()->GetDragHandle()->gesture_nudge_target_visibility());
  EXPECT_FALSE(GetShelfWidget()
                   ->GetDragHandle()
                   ->has_show_drag_handle_timer_for_testing());
}

}  // namespace ash
