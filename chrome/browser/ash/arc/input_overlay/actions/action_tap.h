// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"

namespace arc::input_overlay {

class TouchInjector;

// ActionTap transform key/mouse events to touch events.
class ActionTap : public Action {
 public:
  explicit ActionTap(TouchInjector* touch_injector);
  ActionTap(const ActionTap&) = delete;
  ActionTap& operator=(const ActionTap&) = delete;
  ~ActionTap() override;

  // Action:
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

 private:
  class ActionTapView;

  // Json value format:
  // {
  //   "id": 0,
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
  bool ParseJsonFromKeyboard(const base::Value::Dict& value);
  // Json value format:
  // {
  //   "id": 0,
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
  bool ParseJsonFromMouse(const base::Value::Dict& value);
  bool RewriteKeyEvent(const ui::KeyEvent* key_event,
                       const gfx::RectF& content_bounds,
                       const gfx::Transform* rotation_transform,
                       std::list<ui::TouchEvent>& rewritten_events,
                       bool& keep_original_event);
  bool RewriteMouseEvent(const ui::MouseEvent* mouse_event,
                         const gfx::RectF& content_bounds,
                         const gfx::Transform* rotation_transform,
                         std::list<ui::TouchEvent>& rewritten_events);
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_H_
