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
          "key": "KeyP",
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

class ActionViewTest : public views::ViewsTestBase {
 protected:
  ActionViewTest() = default;

  void PressLeftMouseAtActionView() {
    // Press down at the center of the touch point.
    local_location_ = move_action_view_->touch_point()->bounds().CenterPoint();
    const auto root_location = move_action_->touch_down_positions()[0];
    root_location_ = gfx::Point((int)root_location.x(), (int)root_location.y());
    auto press =
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, local_location_, root_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    move_action_view_->touch_point()->ApplyMousePressed(press);
  }

  void MouseDragActionViewBy(const gfx::Vector2d& move) {
    local_location_ += move;
    root_location_ += move;
    auto drag =
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, local_location_, root_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    move_action_view_->touch_point()->ApplyMouseDragged(drag);
  }

  void ReleaseLeftMouse() {
    auto release =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, local_location_, root_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    move_action_view_->touch_point()->ApplyMouseReleased(release);
  }

  void TouchPressAtActionView() {
    // Press down at the center of the touch point, which is the touch down
    // position.
    const auto& root_location = move_action_->touch_down_positions()[0];
    root_location_ = gfx::Point((int)root_location.x(), (int)root_location.y());

    auto scroll_begin = ui::GestureEvent(
        root_location_.x(), root_location_.y(), ui::EF_NONE,
        base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, 0));
    move_action_view_->touch_point()->ApplyGestureEvent(&scroll_begin);
  }

  void TouchMoveAtActionViewBy(const gfx::Vector2d& move) {
    root_location_ += move;
    auto scroll_update =
        ui::GestureEvent(root_location_.x(), root_location_.y(), ui::EF_NONE,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE,
                                                 move.x(), move.y()));
    move_action_view_->touch_point()->ApplyGestureEvent(&scroll_update);
  }

  void TouchReleaseAtActionView() {
    auto scroll_end =
        ui::GestureEvent(root_location_.x(), root_location_.y(), ui::EF_NONE,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
    move_action_view_->touch_point()->ApplyGestureEvent(&scroll_end);
  }

  void SetDisplayMode(DisplayMode display_mode) {
    input_mapping_view_->SetDisplayMode(display_mode);
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
    widget_ = CreateArcWindow(root_window(), gfx::Rect(200, 100, 400, 600));
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
    move_action_ = actions[0].get();
    move_action_view_ =
        static_cast<ActionView*>(input_mapping_view_->children()[0]);
    tap_action_ = actions[1].get();
    tap_action_view_ =
        static_cast<ActionView*>(input_mapping_view_->children()[0]);
    input_mapping_view_->SetDisplayMode(DisplayMode::kEdit);
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

TEST_F(ActionViewTest, TestDragMove) {
  // Drag move by mouse.
  auto updated_pos = move_action_->touch_down_positions()[0];
  PressLeftMouseAtActionView();
  auto origin_mouse_pos = root_location_;
  MouseDragActionViewBy(gfx::Vector2d(50, 60));
  ReleaseLeftMouse();
  // Save the change.
  move_action_->BindPending();
  auto mouse_moved = root_location_ - origin_mouse_pos;
  updated_pos += mouse_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);

  // Drag move by touch.
  updated_pos = move_action_->touch_down_positions()[0];
  TouchPressAtActionView();
  auto origin_touch_pos = root_location_;
  TouchMoveAtActionViewBy(gfx::Vector2d(-10, -15));
  TouchReleaseAtActionView();
  // Save the change.
  move_action_->BindPending();
  auto touch_moved = root_location_ - origin_touch_pos;
  updated_pos += touch_moved;
  // Check if touch position is updated after drag move.
  EXPECT_POINTF_NEAR(updated_pos, move_action_->touch_down_positions()[0],
                     kTolerance);
}

TEST_F(ActionViewTest, TestArrowKeyMove) {
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

TEST_F(ActionViewTest, TestDisplayModeChange) {
  // From AlphaV2, in edit mode, tap action label is centered on the top of the
  // touch point in view mode and has offset to the touch point in the edit
  // mode.
  auto* touch_point = tap_action_view_->touch_point();
  // Test is set up with edit mode.
  EXPECT_NE(touch_point, nullptr);
  auto expected_label_bottom_right_pos = touch_point->origin();
  expected_label_bottom_right_pos -=
      gfx::Vector2d(kOffsetToTouchPoint, kOffsetToTouchPoint);
  // Default label position is on the top-left of touch point since AlphaV2.
  auto* label = tap_action_view_->labels()[0];
  EXPECT_EQ(expected_label_bottom_right_pos, label->bounds().bottom_right());
  SetDisplayMode(DisplayMode::kView);
  EXPECT_EQ(tap_action_view_->touch_point(), nullptr);
  EXPECT_EQ(tap_action_view_->touch_point_center(),
            label->bounds().CenterPoint());
}

}  // namespace
}  // namespace arc::input_overlay
