// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

#include "ash/public/cpp/window_properties.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/views/test/views_test_base.h"

namespace arc::input_overlay {
namespace {

// Consider two points are at the same position within kTolerance.
constexpr const float kTolerance = 0.999f;

constexpr const char kValidJsonActionMoveKey[] =
    R"json({
      "move": [
        {
          "id": 0,
          "input_sources": [
            "keyboard"
          ],
          "name": "Virtual Joystick",
          "keys": [
            "KeyW",
            "KeyA",
            "KeyS",
            "KeyD"
          ],
          "location": [
            {
              "type": "position",
              "anchor": [
                0,
                0
              ],
              "anchor_to_target": [
                0.5,
                0.5
              ]
            }
          ]
        }
      ],
      "tap": [
        {
          "id": 1,
          "input_sources": [
            "keyboard"
          ],
          "name": "Fight",
          "key": "Space",
          "location": [
            {
              "type": "position",
              "anchor": [
                0,
                0
              ],
              "anchor_to_target": [
                0.5,
                0.5
              ]
            },
            {
              "type": "position",
              "anchor": [
                0,
                0
              ],
              "anchor_to_target": [
                0.3,
                0.3
              ]
            }
          ]
        }
      ]
    })json";

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
      NOTREACHED();
  }
}

class ActionViewTest : public views::ViewsTestBase {
 protected:
  ActionViewTest() = default;

  void PressLeftMouseAtActionView(ActionView* action_view) {
    // Press down at the center of the touch point.
    local_location_ = action_view->touch_point()->bounds().CenterPoint();
    const auto root_location = action_view->action()->touch_down_positions()[0];
    root_location_ = gfx::Point((int)root_location.x(), (int)root_location.y());
    auto press =
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, local_location_, root_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    action_view->touch_point()->ApplyMousePressed(press);
  }

  void MouseDragActionViewBy(ActionView* action_view,
                             const gfx::Vector2d& move) {
    local_location_ += move;
    root_location_ += move;
    auto drag =
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, local_location_, root_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    action_view->touch_point()->ApplyMouseDragged(drag);
  }

  void ReleaseLeftMouse(ActionView* action_view) {
    auto release =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, local_location_, root_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    action_view->touch_point()->ApplyMouseReleased(release);
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
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0));
    action_view->touch_point()->ApplyGestureEvent(&scroll_begin);
  }

  void TouchMoveAtActionViewBy(ActionView* action_view,
                               const gfx::Vector2d& move) {
    root_location_ += move;
    auto scroll_update =
        ui::GestureEvent(root_location_.x(), root_location_.y(), ui::EF_NONE,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE,
                                                 move.x(), move.y()));
    action_view->touch_point()->ApplyGestureEvent(&scroll_update);
  }

  void TouchReleaseAtActionView(ActionView* action_view) {
    auto scroll_end =
        ui::GestureEvent(root_location_.x(), root_location_.y(), ui::EF_NONE,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
    action_view->touch_point()->ApplyGestureEvent(&scroll_end);
  }

  void SetDisplayMode(DisplayMode display_mode) {
    input_mapping_view_->SetDisplayMode(display_mode);
  }

  absl::optional<size_t> GetIndexOf(const views::View* view) const {
    return input_mapping_view_->GetIndexOf(view);
  }

  raw_ptr<ActionView> move_action_view_;
  raw_ptr<ActionView> tap_action_view_;
  raw_ptr<Action> move_action_;
  raw_ptr<Action> tap_action_;
  gfx::Point root_location_;
  gfx::Point local_location_;

 private:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    root_window()->SetBounds(gfx::Rect(1000, 800));
    widget_ = CreateArcWindow(root_window(), gfx::Rect(200, 100, 600, 400));
    touch_injector_ = std::make_unique<TouchInjector>(
        widget_->GetNativeWindow(),
        *widget_->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey),
        base::BindLambdaForTesting(
            [&](std::unique_ptr<AppDataProto>, std::string) {}));
    touch_injector_->set_allow_reposition(true);
    touch_injector_->ParseActions(
        *base::JSONReader::ReadAndReturnValueWithError(
            kValidJsonActionMoveKey));
    touch_injector_->RegisterEventRewriter();
    display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
        touch_injector_.get(), false);
    input_mapping_view_ =
        std::make_unique<InputMappingView>(display_overlay_controller_.get());

    const auto& actions = touch_injector_->actions();
    DCHECK_EQ(2u, actions.size());
    // ActionTap is added first.
    tap_action_ = actions[0].get();
    tap_action_view_ = tap_action_->action_view();
    move_action_ = actions[1].get();
    move_action_view_ = move_action_->action_view();
    SetDisplayMode(DisplayMode::kView);
  }

  void TearDown() override {
    move_action_view_ = nullptr;
    move_action_ = nullptr;
    display_overlay_controller_.reset();
    touch_injector_.reset();
    input_mapping_view_.reset();
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<InputMappingView> input_mapping_view_;
  std::unique_ptr<TouchInjector> touch_injector_;
  std::unique_ptr<DisplayOverlayController> display_overlay_controller_;
};

TEST_F(ActionViewTest, TestDragMoveActionMove) {
  SetDisplayMode(DisplayMode::kEdit);
  // Drag move by mouse.
  auto updated_pos = move_action_->touch_down_positions()[0];
  PressLeftMouseAtActionView(move_action_view_);
  auto origin_mouse_pos = root_location_;
  MouseDragActionViewBy(move_action_view_, gfx::Vector2d(50, 60));
  ReleaseLeftMouse(move_action_view_);
  // Save the change.
  move_action_->BindPending();
  auto mouse_moved = root_location_ - origin_mouse_pos;
  updated_pos += mouse_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);

  // Drag move by touch.
  updated_pos = move_action_->touch_down_positions()[0];
  TouchPressAtActionView(move_action_view_);
  auto origin_touch_pos = root_location_;
  TouchMoveAtActionViewBy(move_action_view_, gfx::Vector2d(-10, -15));
  TouchReleaseAtActionView(move_action_view_);
  // Save the change.
  move_action_->BindPending();
  auto touch_moved = root_location_ - origin_touch_pos;
  updated_pos += touch_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);
}

TEST_F(ActionViewTest, TestDisplayModeChange) {
  // In view mode.
  auto* label = tap_action_view_->labels()[0];
  EXPECT_EQ(label->size(), tap_action_view_->size());
  const auto expected_touch_center = tap_action_view_->GetTouchCenterInWindow();
  // In edit mode.
  SetDisplayMode(DisplayMode::kEdit);
  auto* touch_point = tap_action_view_->touch_point();
  EXPECT_TRUE(touch_point);
  auto touch_point_in_window = touch_point->bounds().CenterPoint();
  touch_point_in_window.Offset(tap_action_view_->origin().x(),
                               tap_action_view_->origin().y());
  EXPECT_EQ(expected_touch_center, touch_point_in_window);
  auto tap_view_bounds = tap_action_view_->bounds();
  EXPECT_TRUE(tap_view_bounds.Contains(touch_point_in_window));
  // Change key binding.
  ui::KeyEvent event_a(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  label->OnKeyPressed(event_a);
  EXPECT_EQ(tap_view_bounds, tap_action_view_->bounds());
  touch_point_in_window = touch_point->bounds().CenterPoint();
  touch_point_in_window.Offset(tap_action_view_->origin().x(),
                               tap_action_view_->origin().y());
  EXPECT_EQ(expected_touch_center, touch_point_in_window);
  tap_view_bounds = tap_action_view_->bounds();
  EXPECT_TRUE(tap_view_bounds.Contains(touch_point_in_window));
  auto label_position = label->origin();
  label_position.Offset(tap_action_view_->origin().x(),
                        tap_action_view_->origin().y());
  tap_action_->BindPending();
  // In view mode.
  SetDisplayMode(DisplayMode::kView);
  auto label_position_new = label->origin();
  label_position_new.Offset(tap_action_view_->origin().x(),
                            tap_action_view_->origin().y());
  EXPECT_EQ(label_position, label_position_new);
}

TEST_F(ActionViewTest, TestDragMoveActionTap) {
  SetDisplayMode(DisplayMode::kEdit);
  const auto* touch_point = tap_action_view_->touch_point();
  const auto* label = tap_action_view_->labels()[0];
  // Check initial position.
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);
  // Drag move by mouse.
  auto updated_pos = tap_action_->touch_down_positions()[0];
  PressLeftMouseAtActionView(tap_action_view_);
  auto origin_mouse_pos = root_location_;
  MouseDragActionViewBy(tap_action_view_, gfx::Vector2d(-10, 0));
  ReleaseLeftMouse(tap_action_view_);
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
  TouchPressAtActionView(tap_action_view_);
  auto origin_touch_pos = root_location_;
  TouchMoveAtActionViewBy(tap_action_view_, gfx::Vector2d(20, 0));
  TouchReleaseAtActionView(tap_action_view_);
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
  // Mouse drag to the right edge. T represents the |tap_action_view_| position.
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
  const auto& available_size = tap_action_view_->parent()->size();
  PressLeftMouseAtActionView(tap_action_view_);
  auto touch_point_in_window = touch_point->origin();
  touch_point_in_window.Offset(tap_action_view_->origin().x(),
                               tap_action_view_->origin().y());
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);

  // Mouse drag to the left edge.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |T |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(-available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);

  // Mouse drag to the middle-left.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |  T       |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(available_size.width() / 3, 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kTopLeft, touch_point, label);

  // Mouse drag to the middle-right.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |       T  |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(available_size.width() / 3, 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);

  // Mouse drag to the top edge.
  //  |----------------|
  //  |          T     |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(0, -available_size.height()));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomRight, touch_point,
                              label);

  // Mouse drag to the top-right corner.
  //  |----------------|
  //  |              T |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomLeft, touch_point,
                              label);

  // Mouse drag to the top edge on the middle-left.
  //  |----------------|
  //  |    T           |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(-2 * available_size.width() / 3, 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomLeft, touch_point,
                              label);

  // Mouse drag to the top-left corner.
  //  |----------------|
  //  |T               |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |__|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(-available_size.width(), 0));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kBottomRight, touch_point,
                              label);

  // Mouse drag to the bottom-left corner.
  //  |----------------|
  //  |                |
  //  |  |----------|  |
  //  |  |          |  |
  //  |  |          |  |
  //  |T_|__________|__|
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_,
                        gfx::Vector2d(0, available_size.height()));
  ReleaseLeftMouse(tap_action_view_);
  CheckActionTapLabelPosition(TapLabelPosition::kTopRight, touch_point, label);
}

TEST_F(ActionViewTest, TestArrowKeyMove) {
  SetDisplayMode(DisplayMode::kEdit);
  // Arrow key left single press & release.
  auto updated_pos = move_action_->touch_down_positions()[0];
  move_action_view_->touch_point()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_LEFT, ui::EF_NONE));
  move_action_view_->touch_point()->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_LEFT, ui::EF_NONE));
  move_action_->BindPending();
  auto move_left = gfx::Vector2d(-kArrowKeyMoveDistance, 0);
  updated_pos += move_left;
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);

  // Arrow key down single press & release.
  updated_pos = move_action_->touch_down_positions()[0];
  move_action_view_->touch_point()->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_DOWN, ui::EF_NONE));
  move_action_view_->touch_point()->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_DOWN, ui::EF_NONE));
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
    move_action_view_->touch_point()->OnKeyPressed(
        ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RIGHT, ui::EF_NONE));
    updated_pos += move_right;
  }
  move_action_view_->touch_point()->OnKeyReleased(
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_RIGHT, ui::EF_NONE));
  move_action_->BindPending();
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);
}

TEST_F(ActionViewTest, TestActionViewReorder) {
  SetDisplayMode(DisplayMode::kEdit);
  // Move |move_action_view_| to the right. |tap_action_view_| is sorted in
  // front.
  TouchPressAtActionView(move_action_view_);
  TouchMoveAtActionViewBy(move_action_view_, gfx::Vector2d(20, 0));
  TouchReleaseAtActionView(move_action_view_);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(0u, *GetIndexOf(tap_action_view_));
  EXPECT_EQ(1u, *GetIndexOf(move_action_view_));
  // Move |tap_action_view_| to the right of |move_action_view_|.
  // |move_action_view_| is sorted in front.
  TouchPressAtActionView(tap_action_view_);
  TouchMoveAtActionViewBy(tap_action_view_, gfx::Vector2d(30, 0));
  TouchReleaseAtActionView(tap_action_view_);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(1u, *GetIndexOf(tap_action_view_));
  EXPECT_EQ(0u, *GetIndexOf(move_action_view_));
  // Move |tap_action_view_| to the top of |move_action_view_|.
  // |tap_action_view_| is sorted in front.
  PressLeftMouseAtActionView(tap_action_view_);
  MouseDragActionViewBy(tap_action_view_, gfx::Vector2d(0, -10));
  ReleaseLeftMouse(tap_action_view_);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(0u, *GetIndexOf(tap_action_view_));
  EXPECT_EQ(1u, *GetIndexOf(move_action_view_));
  // Move |move_action_view_| to the left side of the window.
  // |move_action_view_| is sorted in front.
  TouchPressAtActionView(move_action_view_);
  TouchMoveAtActionViewBy(move_action_view_, gfx::Vector2d(-30, 0));
  TouchReleaseAtActionView(move_action_view_);
  SetDisplayMode(DisplayMode::kView);
  SetDisplayMode(DisplayMode::kEdit);
  EXPECT_EQ(1u, *GetIndexOf(tap_action_view_));
  EXPECT_EQ(0u, *GetIndexOf(move_action_view_));
}

}  // namespace
}  // namespace arc::input_overlay
