// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_MOUSE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_MOUSE_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

namespace arc {
namespace input_overlay {
// ActionTapMouse transforms mouse events to touch tap events on predefined
// position. It supports mouse primary click and secondary click. Mouse lock is
// required.
class ActionTapMouse : public Action {
 public:
  explicit ActionTapMouse(aura::Window* window);
  ActionTapMouse(const ActionTapMouse&) = delete;
  ActionTapMouse& operator=(const ActionTapMouse&) = delete;
  ~ActionTapMouse() override;

  // Override from Action.
  // Json value format:
  // {
  //   "name": "any name",
  //   "mouse_action": "primary_click",
  //   "location": [ // Must have at least one position.
  //     {
  //     "type": "position"
  //      ...
  //     }
  //   ]
  // }
  bool ParseFromJson(const base::Value& value) override;
  bool RewriteEvent(const ui::Event& origin,
                    const gfx::RectF& content_bounds,
                    const bool is_mouse_locked,
                    std::list<ui::TouchEvent>& touch_events,
                    bool& keep_original_event) override;
  gfx::PointF GetUIPosition(const gfx::RectF& content_bounds) override;
  std::unique_ptr<ActionLabel> CreateView(
      const gfx::RectF& content_bounds) override;

  const std::string& target_mouse_action() const {
    return target_mouse_action_;
  }
  const base::flat_set<ui::EventType>& target_types() const {
    return target_types_;
  }
  int target_flags() const { return target_flags_; }

 private:
  bool RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                         std::list<ui::TouchEvent>& rewritten_events,
                         const gfx::RectF& content_bounds);

  // "hover_move", "primary_click" and "secondary_click".
  std::string target_mouse_action_;
  // For mouse primary/secondary-click: ET_MOUSE_PRESSED, ET_MOUSE_RELEASED.
  base::flat_set<ui::EventType> target_types_;
  // Mouse primary button flag: EF_LEFT_MOUSE_BUTTON. Secondary button flag:
  // EF_RIGHT_MOUSE_BUTTON.
  int target_flags_ = 0;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_MOUSE_H_
