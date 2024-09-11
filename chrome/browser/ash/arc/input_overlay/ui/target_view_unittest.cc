// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"

#include "ash/shell.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

class TargetViewTest : public OverlayViewTestBase {
 public:
  TargetViewTest() = default;
  ~TargetViewTest() override = default;

  gfx::Point GetTargetCenter(TargetView* target) {
    DCHECK(target);
    return target->center_;
  }

  int GetTargetPadding(TargetView* target) {
    DCHECK(target);
    return target->GetPadding();
  }

  // Convert the point in `TargetView` coordinates to screen coordinates.
  gfx::Point GetPointInScreenFromTargetView(const gfx::Point& point) const {
    DCHECK(touch_injector_);

    const auto& bounds_origin = touch_injector_->content_bounds().origin();
    gfx::Point point_in_screen = point;
    point_in_screen.Offset(bounds_origin.x(), bounds_origin.y());
    return point_in_screen;
  }

  // Verifies whether `touch_injector` has `expect_size` actions and the last
  // action has `expect_position`.
  void VerifyLastActionPosition(size_t expect_size,
                                const gfx::Point& expect_position) {
    DCHECK(touch_injector_);

    // Verify action view size.
    EXPECT_GE(expect_size, 1u);
    EXPECT_EQ(expect_size, GetActionViewSize());

    // Verify action size.
    const auto& actions = touch_injector_->actions();
    EXPECT_EQ(expect_size, actions.size());

    // Verify last action position.
    const auto* new_action = actions[expect_size - 1].get();
    const auto& positions = new_action->touch_down_positions();
    EXPECT_EQ(1u, positions.size());
    EXPECT_EQ(gfx::PointF(expect_position), positions[0]);
  }
};

TEST_F(TargetViewTest, TestInitialCursorLocation) {
  PressAddButton();
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  EXPECT_EQ(target_view->GetBoundsInScreen().CenterPoint(),
            aura::Env::GetInstance()->last_mouse_location());
}

TEST_F(TargetViewTest, TestMouseSupport) {
  // Enter into the button placement mode and check mouse hover move and click.
  PressAddButton();
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);

  auto* event_generator = GetEventGenerator();
  auto global_center = target_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(global_center);
  auto local_center = GetTargetCenter(target_view);
  EXPECT_EQ(global_center, GetPointInScreenFromTargetView(local_center));

  // Move mouse by `dx` and `dy`.
  const int dx = 5;
  const int dy = 10;
  event_generator->MoveMouseBy(/*x=*/dx, /*y=*/dy);
  global_center.Offset(/*delta_x=*/dx, /*delta_y=*/dy);
  local_center.Offset(/*delta_x=*/dx, /*delta_y=*/dy);
  EXPECT_EQ(local_center, GetTargetCenter(target_view));
  EXPECT_EQ(global_center, GetPointInScreenFromTargetView(local_center));

  const size_t action_view_size = GetActionViewSize();
  // Mouse click to drop down the action.
  event_generator->ClickLeftButton();
  // Check if the action is dropped on the expected position.
  VerifyLastActionPosition(action_view_size + 1,
                           GetPointInScreenFromTargetView(local_center));
}

TEST_F(TargetViewTest, TestCenterClamp) {
  // Enter into the button placement mode and check when mouse hover moved
  // to outside of the window.
  PressAddButton();
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(target_view->GetBoundsInScreen().CenterPoint());
  // Simulate moving mouse to the outside of the window on the left.
  for (int i = 0; i < target_view->size().width(); i++) {
    event_generator->MoveMouseBy(/*x=*/-1, /*y=*/0);
  }
  // Stay in the button placement mode and the target center should still stay
  // inside and show the complete circle.
  target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  EXPECT_EQ(GetTargetCenter(target_view).x(), GetTargetPadding(target_view));
}

TEST_F(TargetViewTest, TestKeyboardSupport) {
  // Enter into the button placement mode.
  PressAddButton();
  EXPECT_TRUE(GetTargetView());

  // Press a random key and it is still in the button placement mode.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_A, ui::EF_NONE);
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);

  // Press the key `left` twice to update the target center.
  auto local_center = GetTargetCenter(target_view);
  local_center.Offset(-2 * kArrowKeyMoveDistance, 0);
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_LEFT, ui::EF_NONE);
  event_generator->ReleaseKey(ui::VKEY_LEFT, ui::EF_NONE);
  target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  EXPECT_EQ(local_center, GetTargetCenter(target_view));

  // Press the key `enter` to place down the action.
  size_t action_view_size = GetActionViewSize();
  event_generator->PressAndReleaseKey(ui::VKEY_RETURN, ui::EF_NONE);
  EXPECT_FALSE(GetTargetView());
  VerifyLastActionPosition(action_view_size + 1,
                           GetPointInScreenFromTargetView(local_center));
  PressDoneButtonOnButtonOptionsMenu();

  // Enter into the button placement mode again and check whether the key `esc`
  // exits the button placement mode without adding anything.
  PressAddButton();
  EXPECT_TRUE(GetTargetView());
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(GetTargetView());
  EXPECT_EQ(action_view_size + 1, GetActionViewSize());
}

TEST_F(TargetViewTest, TestGestureSupport) {
  // Enter into the button placement mode and test the gesture tap on the
  // center.
  PressAddButton();
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  const size_t action_view_size = GetActionViewSize();
  auto global_center = target_view->GetBoundsInScreen().CenterPoint();
  GestureTapOn(target_view);
  EXPECT_FALSE(GetTargetView());
  // Check if the action is dropped on the expect position.
  VerifyLastActionPosition(action_view_size + 1, global_center);
  PressDoneButtonOnButtonOptionsMenu();

  // Enter into the button placement mode and test the gesture scroll.
  PressAddButton();
  target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  auto* event_generator = GetEventGenerator();
  event_generator->PressTouch(global_center);
  auto local_center = GetTargetCenter(target_view);
  EXPECT_EQ(global_center, GetPointInScreenFromTargetView(local_center));

  const int dx = 3;
  const int dy = 4;
  global_center.Offset(/*delta_x=*/dx, /*delta_y=*/dy);
  local_center.Offset(/*delta_x=*/dx, /*delta_y=*/dy);
  event_generator->MoveTouch(global_center);
  global_center.Offset(/*delta_x=*/dx, /*delta_y=*/dy);
  local_center.Offset(/*delta_x=*/dx, /*delta_y=*/dy);
  // Touch move more than once to trigger scroll gesture.
  event_generator->MoveTouch(global_center);
  EXPECT_TRUE(GetTargetView());
  EXPECT_EQ(local_center, GetTargetCenter(target_view));
  EXPECT_EQ(global_center, GetPointInScreenFromTargetView(local_center));

  event_generator->ReleaseTouch();
  // Check if the action is dropped on the expected position.
  VerifyLastActionPosition(action_view_size + 2,
                           GetPointInScreenFromTargetView(local_center));
}

TEST_F(TargetViewTest, TestMultiDisplay) {
  UpdateDisplay("1000x900,1000x900");
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  display::Display display1 = display::Screen::GetScreen()->GetDisplayMatching(
      root_windows[1]->GetBoundsInScreen());

  // Move the window from the primary display to `display1`.
  auto* arc_window = widget_->GetNativeWindow();
  ASSERT_TRUE(arc_window);
  auto screen_bounds = arc_window->bounds();
  const auto& display_bounds = display1.bounds();
  screen_bounds.Offset(display_bounds.x(), display_bounds.y());
  arc_window->SetBoundsInScreen(screen_bounds, display1);
  // Update `controller_` since it is updated from switching display.
  controller_ = GetDisplayOverlayController();

  // Show `target_view` and it should show inside of the window bounds.
  EnableEditMode();
  PressAddButton();
  auto* target_view = GetTargetView();
  EXPECT_TRUE(target_view);
  EXPECT_TRUE(screen_bounds.Contains(target_view->GetBoundsInScreen()));
}

}  // namespace arc::input_overlay
