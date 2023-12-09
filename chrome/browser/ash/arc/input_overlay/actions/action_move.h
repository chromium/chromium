// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/aura/window.h"

namespace arc::input_overlay {
// UI specs.
constexpr int kActionMoveMinRadius = 99;

class TouchInjector;

// ActionMoveKey transforms key/mouse events to touch events with touch
// move involved.
class ActionMove : public Action {
 public:
  explicit ActionMove(TouchInjector* touch_injector);
  ActionMove(const ActionMove&) = delete;
  ActionMove& operator=(const ActionMove&) = delete;
  ~ActionMove() override;

  // Override from Action.
  bool ParseFromJson(const base::Value::Dict& value) override;
  bool InitByAddingNewAction(const gfx::Point& target_pos) override;
  void InitByChangingActionType(Action* action) override;
  bool RewriteEvent(const ui::Event& origin,
                    const bool is_mouse_locked,
                    const gfx::Transform* rotation_transform,
                    std::list<ui::TouchEvent>& touch_events,
                    bool& keep_original_event) override;
  gfx::PointF GetUICenterPosition() override;
  std::unique_ptr<ActionView> CreateView(
      DisplayOverlayController* display_overlay_controller) override;
  void UnbindInput(const InputElement& input_element) override;
  std::unique_ptr<ActionProto> ConvertToProtoIfCustomized() const override;
  ActionType GetType() const override;

  void set_move_distance(int move_distance) { move_distance_ = move_distance; }
  int move_distance() { return move_distance_; }

 private:
  class ActionMoveKeyView;
  class ActionMoveMouseView;

  // Json value format:
  // {
  //   "id": 0,
  //   "name": "WASD",
  //   "input_sources": [
  //     "keyboard"
  //   ],
  //   "key": ["KeyW", "KeyA", "KeyS", "KeyD"],
  //   "location": [
  //     {
  //       "type": "position",
  //       ...
  //     },
  //     {}
  //   ]
  // }
  bool ParseJsonFromKeyboard(const base::Value::Dict& value);
  // Json value format:
  // {
  //   "id": 0,
  //   "name": "camera move",
  //   "input_sources": [
  //     "mouse"
  //   ],
  //   "mouse_action": "hover_move", // primary_drag_move, secondary_drag_move
  //   "location": [ // optional
  //     {
  //       "type": "position",
  //       ...
  //     }
  //   ],
  //   "target_area": { // optional
  //     "top_left": {
  //         "type": "position",
  //          ...
  //       },
  //     "bottom_right":
  //       {}
  //   }
  // }
  bool ParseJsonFromMouse(const base::Value::Dict& value);
  bool RewriteKeyEvent(const ui::KeyEvent* key_event,
                       const gfx::RectF& content_bounds,
                       const gfx::Transform* rotation_transform,
                       std::list<ui::TouchEvent>& rewritten_events);
  bool RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                         const gfx::RectF& content_bounds,
                         const gfx::Transform* rotation_transform,
                         std::list<ui::TouchEvent>& rewritten_events);

  // For key-bound move.
  void CalculateMoveVector(gfx::PointF& touch_press_pos,
                           size_t direction_index,
                           bool key_press,
                           const gfx::RectF& content_bounds,
                           const gfx::Transform* rotation_transform);

  // For mouse-bound move.
  // Return the bounds in the root window.
  std::optional<gfx::RectF> CalculateApplyArea(const gfx::RectF& content_bound);
  // Transform mouse location from app window to the `target_area_` if
  // `target_area_` exists. Input values are in root window's coordinate system.
  // Return the point pixel to the host window's.
  gfx::PointF TransformLocationInPixels(const gfx::RectF& content_bounds,
                                        const gfx::PointF& point);
  // Move distance saves the distance moving from its center in any direction
  // for key-binding.
  int move_distance_ = kActionMoveMinRadius / 2;
  // Move vector saves the direction with distance moving from its center in any
  // direction for key-binding.
  gfx::Vector2dF move_vector_;

  // Some games defines a specific area with touch move effects, for example,
  // left half window or right half window. If it is null, the whole window will
  // be the apply area.
  std::vector<std::unique_ptr<Position>> target_area_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_H_
