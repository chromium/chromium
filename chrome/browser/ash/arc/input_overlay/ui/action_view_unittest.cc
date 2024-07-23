// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace arc::input_overlay {
namespace {

// Consider two points are at the same position within kTolerance.
constexpr const float kTolerance = 0.999f;

// Check label offset in edit mode.
void CheckActionTapLabelPosition(TapLabelPosition label_position,
                                 const TouchPoint* touch_point,
                                 const ActionLabel* label) {
  DCHECK_NE(label_position, TapLabelPosition::kNone);
  DCHECK(touch_point);
  switch (label_position) {
    case TapLabelPosition::kTopLeft: {
      auto expected_label_bottom_right_pos = touch_point->origin();
      expected_label_bottom_right_pos -=
          gfx::Vector2d(kOffsetToTouchPoint, kOffsetToTouchPoint);
      EXPECT_EQ(expected_label_bottom_right_pos,
                label->bounds().bottom_right());
    } break;
    case TapLabelPosition::kTopRight: {
      auto expected_label_bottom_left_pos = touch_point->bounds().top_right();
      expected_label_bottom_left_pos -=
          gfx::Vector2d(-kOffsetToTouchPoint, kOffsetToTouchPoint);
      EXPECT_EQ(expected_label_bottom_left_pos, label->bounds().bottom_left());
    } break;
    case TapLabelPosition::kBottomLeft: {
      auto expected_label_top_right_pos = touch_point->bounds().bottom_left();
      expected_label_top_right_pos -=
          gfx::Vector2d(kOffsetToTouchPoint, -kOffsetToTouchPoint);
      EXPECT_EQ(expected_label_top_right_pos, label->bounds().top_right());
    } break;
    case TapLabelPosition::kBottomRight: {
      auto expected_label_top_left_pos = touch_point->bounds().bottom_right();
      expected_label_top_left_pos -=
          gfx::Vector2d(-kOffsetToTouchPoint, -kOffsetToTouchPoint);
      EXPECT_EQ(expected_label_top_left_pos, label->bounds().origin());
    } break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

class ActionViewTest : public OverlayViewTestBase {
 public:
  ActionViewTest() = default;
  ~ActionViewTest() override = default;

 protected:
  void PressLeftMouseAtActionView(ActionView* action_view) {
    // Press down at the center of the touch point.
    local_location_ = action_view->touch_point()->bounds().CenterPoint();
    const auto root_location = action_view->action()->touch_down_positions()[0];
    root_location_ = gfx::Point((int)root_location.x(), (int)root_location.y());
    auto press =
        ui::MouseEvent(ui::EventType::kMousePressed, local_location_,
                       root_location_, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    action_view->touch_point()->OnMousePressed(press);
  }

  void MouseDragActionViewBy(ActionView* action_view,
                             const gfx::Vector2d& move) {
    local_location_ += move;
    root_location_ += move;
    auto drag = ui::MouseEvent(ui::EventType::kMouseDragged, local_location_,
                               root_location_, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
    action_view->touch_point()->OnMouseDragged(drag);
  }

  void ReleaseLeftMouse(ActionView* action_view) {
    auto release =
        ui::MouseEvent(ui::EventType::kMouseReleased, local_location_,
                       root_location_, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    action_view->touch_point()->OnMouseReleased(release);
  }

  void TouchPressAtActionView(ActionView* action_view) {
    // Press down at the center of the touch point, which is the touch down
    // position.
    const auto& root_location =
        action_view->action()->touch_down_positions()[0];
    root_location_ = gfx::Point((int)root_location.x(), (int)root_location.y());

    auto scroll_begin = ui::GestureEvent(
        root_location_.x(), root_location_.y(), ui::EF_NONE,
        base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, 0));
    action_view->touch_point()->OnGestureEvent(&scroll_begin);
  }

  void TouchMoveAtActionViewBy(ActionView* action_view,
                               const gfx::Vector2d& move) {
    root_location_ += move;
    auto scroll_update = ui::GestureEvent(
        root_location_.x(), root_location_.y(), ui::EF_NONE,
        base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, move.x(),
                                move.y()));
    action_view->touch_point()->OnGestureEvent(&scroll_update);
  }

  void TouchReleaseAtActionView(ActionView* action_view) {
    auto scroll_end = ui::GestureEvent(
        root_location_.x(), root_location_.y(), ui::EF_NONE,
        base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
    action_view->touch_point()->OnGestureEvent(&scroll_end);
  }

  void SetDisplayMode(DisplayMode display_mode) {
    input_mapping_view_->SetDisplayMode(display_mode);
  }

  std::optional<size_t> GetIndexOf(const views::View* view) const {
    return input_mapping_view_->GetIndexOf(view);
  }

  gfx::Point local_location_;
  gfx::Point root_location_;
};

TEST_F(ActionViewTest, TestDragMoveActionMove) {
  SetDisplayMode(DisplayMode::kEdit);
  auto* move_action_view = move_action_->action_view();
  // Drag move by mouse.
  auto updated_pos = move_action_->touch_down_positions()[0];
  PressLeftMouseAtActionView(move_action_view);
  auto origin_mouse_pos = root_location_;
  MouseDragActionViewBy(move_action_view, gfx::Vector2d(10, 10));
  ReleaseLeftMouse(move_action_view);
  // Save the change.
  move_action_->BindPending();
  auto mouse_moved = root_location_ - origin_mouse_pos;
  updated_pos += mouse_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);

  // Drag move by touch.
  updated_pos = move_action_->touch_down_positions()[0];
  TouchPressAtActionView(move_action_view);
  auto origin_touch_pos = root_location_;
  TouchMoveAtActionViewBy(move_action_view, gfx::Vector2d(-10, -15));
  TouchReleaseAtActionView(move_action_view);
  // Save the change.
  move_action_->BindPending();
  auto touch_moved = root_location_ - origin_touch_pos;
  updated_pos += touch_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);
}

TEST_F(ActionViewTest, TestDragMoveActionTap) {
  SetDisplayMode(DisplayMode::kEdit);
  auto* tap_action_view = tap_action_->action_view();
  const auto* touch_point = tap_action_view->touch_point();
  const auto* label = tap_action_view->labels()[0].get();
  // Check initial position.
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);
  // Drag move by mouse.
  auto updated_pos = tap_action_->touch_down_positions()[0];
  PressLeftMouseAtActionView(tap_action_view);
  auto origin_mouse_pos = root_location_;
  MouseDragActionViewBy(tap_action_view, gfx::Vector2d(-10, 0));
  ReleaseLeftMouse(tap_action_view);
  // Save the change.
  tap_action_->BindPending();
  auto mouse_moved = root_location_ - origin_mouse_pos;
  updated_pos += mouse_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, tap_action_->touch_down_positions()[0],
                     kTolerance);
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);

  // Drag move by touch.
  updated_pos = tap_action_->touch_down_positions()[0];
  TouchPressAtActionView(tap_action_view);
  auto origin_touch_pos = root_location_;
  TouchMoveAtActionViewBy(tap_action_view, gfx::Vector2d(20, 0));
  TouchReleaseAtActionView(tap_action_view);
  // Save the change.
  tap_action_->BindPending();
  auto touch_moved = root_location_ - origin_touch_pos;
  updated_pos += touch_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, tap_action_->touch_down_positions()[0],
                     kTolerance);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);

  // The label position has different label offset positions depending on the
  // current position.
  // Mouse drag to the right edge. T represents the `tap_action_view` position.
  // From
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |       T  |  |
  //  |  |          |  |
  //  |__|__________|__|
  // To
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |          | T|
  //  |  |          |  |
  //  |__|__________|__|
  const auto& available_size = tap_action_view->parent()->size();
  PressLeftMouseAtActionView(tap_action_view);
  auto touch_point_in_window = touch_point->origin();
  touch_point_in_window.Offset(tap_action_view->origin().x(),
                               tap_action_view->origin().y());
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);

  // Mouse drag to the left edge.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |T |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(-available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);

  // Mouse drag to the middle-left.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |  T       |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(available_size.width() / 3, 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);

  // Mouse drag to the middle-right.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |       T  |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(available_size.width() / 3, 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);

  // Mouse drag to the top edge.
  //  |----------------|
  //  |          T     |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(0, -available_size.height()));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomRight, touch_point,
                              label);

  // Mouse drag to the top-right corner.
  //  |----------------|
  //  |              T |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomLeft, touch_point,
                              label);

  // Mouse drag to the top edge on the middle-left.
  //  |----------------|
  //  |    T           |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(-2 * available_size.width() / 3, 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomLeft, touch_point,
                              label);

  // Mouse drag to the top-left corner.
  //  |----------------|
  //  |T               |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(-available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomRight, touch_point,
                              label);

  // Mouse drag to the bottom-left corner.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |T_|__________|__|
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view,
                        gfx::Vector2d(0, available_size.height()));
  ReleaseLeftMouse(tap_action_view);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);
}

TEST_F(ActionViewTest, TestArrowKeyMove) {
  SetDisplayMode(DisplayMode::kEdit);
  auto* move_action_view = move_action_->action_view();
  // Arrow key left single press & release.
  auto updated_pos = move_action_->touch_down_positions()[0];
  move_action_view->touch_point()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_LEFT, ui::EF_NONE));
  move_action_view->touch_point()->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_LEFT, ui::EF_NONE));
  move_action_->BindPending();
  auto move_left = gfx::Vector2d(-kArrowKeyMoveDistance, 0);
  updated_pos += move_left;
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);

  // Arrow key down single press & release.
  updated_pos = move_action_->touch_down_positions()[0];
  move_action_view->touch_point()->OnKeyPressed(
      ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_DOWN, ui::EF_NONE));
  move_action_view->touch_point()->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_DOWN, ui::EF_NONE));
  move_action_->BindPending();
  auto move_down = gfx::Vector2d(0, kArrowKeyMoveDistance);
  updated_pos += move_down;
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);

  // Arrow key right repeat press & release.
  updated_pos = move_action_->touch_down_positions()[0];
  int key_press_times = 5;
  auto move_right = gfx::Vector2d(kArrowKeyMoveDistance, 0);
  for (int i = 0; i < key_press_times; i++) {
    move_action_view->touch_point()->OnKeyPressed(
        ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_RIGHT, ui::EF_NONE));
    updated_pos += move_right;
  }
  move_action_view->touch_point()->OnKeyReleased(
      ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_RIGHT, ui::EF_NONE));
  move_action_->BindPending();
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);
}

TEST_F(ActionViewTest, TestActionViewReorder) {
  SetDisplayMode(DisplayMode::kEdit);
  auto* move_action_view = move_action_->action_view();
  auto* tap_action_view = tap_action_->action_view();
  // Move `tap_action_view` to the right of `move_action_view`.
  // `move_action_view` is sorted in front.
  TouchPressAtActionView(tap_action_view);
  TouchMoveAtActionViewBy(tap_action_view, gfx::Vector2d(10, 0));
  TouchReleaseAtActionView(tap_action_view);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(1u, *GetIndexOf(tap_action_view));
  EXPECT_EQ(0u, *GetIndexOf(move_action_view));
  // Move `move_action_view` to the right. `tap_action_view` is sorted in
  // front.
  TouchPressAtActionView(move_action_view);
  TouchMoveAtActionViewBy(move_action_view, gfx::Vector2d(20, 0));
  TouchReleaseAtActionView(move_action_view);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(0u, *GetIndexOf(tap_action_view));
  EXPECT_EQ(1u, *GetIndexOf(move_action_view));
  // Move `tap_action_view` to the top of `move_action_view`.
  // `tap_action_view` is sorted in front.
  PressLeftMouseAtActionView(tap_action_view);
  MouseDragActionViewBy(tap_action_view, gfx::Vector2d(0, -10));
  ReleaseLeftMouse(tap_action_view);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(0u, *GetIndexOf(tap_action_view));
  EXPECT_EQ(1u, *GetIndexOf(move_action_view));
  // Move `move_action_view` to the left side of the window.
  // `move_action_view` is sorted in front.
  TouchPressAtActionView(move_action_view);
  TouchMoveAtActionViewBy(move_action_view, gfx::Vector2d(-30, 0));
  TouchReleaseAtActionView(move_action_view);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(1u, *GetIndexOf(tap_action_view));
  EXPECT_EQ(0u, *GetIndexOf(move_action_view));
}

}  // namespace arc::input_overlay
