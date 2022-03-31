// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

namespace arc {
namespace input_overlay {
// ActionTap transform key/mouse events to touch events.
class ActionTap : public Action {
 public:
  explicit ActionTap(aura::Window* window);
  ActionTap(const ActionTap&) = delete;
  ActionTap& operator=(const ActionTap&) = delete;
  ~ActionTap() override;

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

 private:
  class ActionTapView;

  // Json value format:
  // {
  //   "input_sources": [
  //     "keyboard"
  //   ],
  //   "name": "Fight",
  //   "key": "KeyA",
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
  //   "name": "any name",
  //   "input_sources": [
  //     "mouse"
  //   ],
  //   "mouse_action": "primary_click",
  //   "location": [ // Must have at least one position.
  //     {
  //     "type": "position"
  //      ...
  //     }
  //   ]
  // }
  bool ParseJsonFromMouse(const base::Value& value);
  bool RewriteKeyEvent(const ui::KeyEvent* key_event,
                       std::list<ui::TouchEvent>& rewritten_events,
                       const gfx::RectF& content_bounds,
                       bool& keep_original_event);
  bool RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                         std::list<ui::TouchEvent>& rewritten_events,
                         const gfx::RectF& content_bounds);
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_H_
