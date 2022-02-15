// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_KEY_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_KEY_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

#include "ui/aura/window.h"

namespace arc {
namespace input_overlay {

// ActionTapKey transforms key event to touch event to simulate touch tap
// action.
class ActionTapKey : public Action {
 public:
  explicit ActionTapKey(aura::Window* window);
  ActionTapKey(const ActionTapKey&) = delete;
  ActionTapKey& operator=(const ActionTapKey&) = delete;
  ~ActionTapKey() override;

  // Override from Action.
  // Json value format:
  // {
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
  bool ParseFromJson(const base::Value& value) override;
  bool RewriteEvent(const ui::Event& origin,
                    const gfx::RectF& content_bounds,
                    const bool is_mouse_locked,
                    std::list<ui::TouchEvent>& touch_events,
                    bool& keep_original_event) override;
  gfx::PointF GetUICenterPosition(const gfx::RectF& content_bounds) override;
  std::unique_ptr<ActionView> CreateView(
      const gfx::RectF& content_bounds) override;

  ui::DomCode key() { return key_; }

 private:
  class ActionTapKeyView;

  bool RewriteKeyEvent(const ui::KeyEvent& key_event,
                       std::list<ui::TouchEvent>& rewritten_events,
                       const gfx::RectF& content_bounds,
                       bool& keep_original_event);
  ui::DomCode key_;
  // |is_modifier_key_| == true is especially for modifier keys (Only Ctrl,
  // Shift and Alt are supported for now) because EventRewriterChromeOS handles
  // specially on modifier key released event by skipping the following event
  // rewriters on key released event. If |is_modifier_key_| == true, touch
  // release event is sent right after touch pressed event for original key
  // pressed event and original modifier key pressed event is also sent as it
  // is. This is only suitable for some UI buttons which don't require keeping
  // press down and change the status only on each touch pressed event instead
  // of changing status on each touch pressed and released event.
  bool is_modifier_key_ = false;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_TAP_KEY_H_
