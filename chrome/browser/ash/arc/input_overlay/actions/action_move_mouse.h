// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_MOUSE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_MOUSE_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

namespace arc {
namespace input_overlay {

// ActionMoveMouse transforms mouse events to touch event to simulate touch move
// events when the mouse is locked.
// It supports below target mouse action.
// 1. Mouse hover move. It requires mouse lock.
// 2&3. Mouse left&right drag move.
// When the mouse is locked, only the targeted mouse events with location inside
// of window content bounds will be processed. The targeted mouse events outside
// of the content bounds will be discarded.
class ActionMoveMouse : public Action {
 public:
  explicit ActionMoveMouse(aura::Window* window);
  ActionMoveMouse(const ActionMoveMouse&) = delete;
  ActionMoveMouse& operator=(const ActionMoveMouse&) = delete;
  ~ActionMoveMouse() override;

  const std::string& target_mouse_action() const {
    return target_mouse_action_;
  }
  const base::flat_set<ui::EventType>& target_types() const {
    return target_types_;
  }
  int target_flags() const { return target_flags_; }

  // Action:
  //
  // Json value format:
  // {
  //   "name": "camera move",
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
  bool ParseFromJson(const base::Value& value) override;
  bool RewriteEvent(const ui::Event& origin,
                    const gfx::RectF& content_bounds,
                    const bool is_mouse_locked,
                    std::list<ui::TouchEvent>& touch_events,
                    bool& keep_original_event) override;
  gfx::PointF GetUICenterPosition(const gfx::RectF& content_bounds) override;
  std::unique_ptr<ActionView> CreateView(
      const gfx::RectF& content_bounds) override;

 private:
  class ActionMoveMouseView;

  bool RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                         const gfx::RectF& content_bounds,
                         const bool is_mouse_locked,
                         std::list<ui::TouchEvent>& rewritten_events);
  // Return the bounds in the root window.
  absl::optional<gfx::RectF> CalculateApplyArea(
      const gfx::RectF& content_bound);
  // Transform mouse location from app window to the |target_area_| if
  // |target_area_| exists. Input values are in root window's coordinate system.
  // Return the point pixel to the host window's.
  gfx::PointF TransformLocationInPixels(const gfx::RectF& content_bounds,
                                        const gfx::PointF& point);

  // "hover_move", "left_drag_move" and "right_drag_move".
  std::string target_mouse_action_;
  // For mouse hover move: ET_MOUSE_ENTERED, ET_MOUSE_MOVED, ET_MOUSE_EXITED.
  // For mouse left/right-click move: ET_MOUSE_PRESSED, ET_MOUSE_DRAGGED,
  // ET_MOUSE_RELEASED.
  base::flat_set<ui::EventType> target_types_;
  // Mouse left button flag: EF_LEFT_MOUSE_BUTTON. Right button flag:
  // EF_RIGHT_MOUSE_BUTTON.
  int target_flags_ = 0;
  // Some games defines a specific area with touch move effects, for example,
  // left half window or right half window. If it is null, the whole window will
  // be the apply area.
  std::vector<std::unique_ptr<Position>> target_area_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_MOUSE_H_
