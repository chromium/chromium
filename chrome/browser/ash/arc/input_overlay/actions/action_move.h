// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/aura/window.h"

namespace arc {
namespace input_overlay {
// UI specs.
constexpr int kActionMoveMinRadius = 99;

// ActionMoveKey transforms key/mouse events to touch events with touch
// move involved.
class ActionMove : public Action {
 public:
  explicit ActionMove(aura::Window* window);
  ActionMove(const ActionMove&) = delete;
  ActionMove& operator=(const ActionMove&) = delete;
  ~ActionMove() override;

  // Override from Action.
  bool ParseFromJson(const base::Value& value) override;
  bool RewriteEvent(const ui::Event& origin,
                    const gfx::RectF& content_bounds,
                    const bool is_mouse_locked,
                    std::list<ui::TouchEvent>& touch_events,
                    bool& keep_original_event) override;
  gfx::PointF GetUICenterPosition(const gfx::RectF& content_bounds) override;
  std::unique_ptr<ActionView> CreateView(
      DisplayOverlayController* display_overlay_controller,
      const gfx::RectF& content_bounds) override;
  bool RequireInputElement(const InputElement& input_element,
                           Action** overlapped_action) override;
  void Unbind() override;

  void set_move_distance(int move_distance) { move_distance_ = move_distance; }
  int move_distance() { return move_distance_; }

 private:
  class ActionMoveKeyView;
  class ActionMoveMouseView;

  // Json value format:
  // {
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
  bool ParseJsonFromKeyboard(const base::Value& value);
  // Json value format:
  // {
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
  bool ParseJsonFromMouse(const base::Value& value);
  bool RewriteKeyEvent(const ui::KeyEvent* key_event,
                       const gfx::RectF& content_bounds,
                       std::list<ui::TouchEvent>& rewritten_events);
  bool RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                         const gfx::RectF& content_bounds,
                         std::list<ui::TouchEvent>& rewritten_events);

  // For key-bound move.
  void CalculateMoveVector(gfx::PointF& touch_press_pos,
                           int direction_index,
                           bool key_press,
                           const gfx::RectF& content_bounds);

  // For mouse-bound move.
  // Return the bounds in the root window.
  absl::optional<gfx::RectF> CalculateApplyArea(
      const gfx::RectF& content_bound);
  // Transform mouse location from app window to the |target_area_| if
  // |target_area_| exists. Input values are in root window's coordinate system.
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

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_H_
