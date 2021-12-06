// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_KEY_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_KEY_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"

namespace arc {
namespace input_overlay {
// Total key size for ActionMoveKey.
constexpr int kActionMoveKeysSize = 4;
// Default move Offset for touch move.
constexpr int kDefaultMoveDistance = 5;

// ActionMoveKey transforms key event to touch event to simulate touch move
// events.
class ActionMoveKey : public Action {
 public:
  explicit ActionMoveKey(aura::Window* window);
  ActionMoveKey(const ActionMoveKey&) = delete;
  ActionMoveKey& operator=(const ActionMoveKey&) = delete;
  ~ActionMoveKey() override;

  // Override from Action.
  // Json value format:
  // {
  //   "name": "WASD",
  //   "key": ["KeyW", "KeyA", "KeyS", "KeyD"],
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
                    std::list<ui::TouchEvent>& touch_events,
                    const gfx::RectF& content_bounds) override;
  gfx::PointF GetUIPosition(const gfx::RectF& content_bounds) override;
  std::unique_ptr<ActionLabel> CreateView(
      const gfx::RectF& content_bounds) override;

  const std::vector<ui::DomCode>& keys() const { return keys_; }

 private:
  bool RewriteKeyEvent(const ui::KeyEvent& key_event,
                       std::list<ui::TouchEvent>& rewritten_events,
                       const gfx::RectF& content_bounds);

  void CalculateMoveVector(gfx::PointF& touch_press_pos,
                           int direction_index,
                           bool key_press,
                           const gfx::RectF& content_bounds);

  // There are four and only four keys representing move up, left, down and
  // right.
  std::vector<ui::DomCode> keys_;
  int move_distance_ = kDefaultMoveDistance;
  gfx::Vector2dF move_vector_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_MOVE_KEY_H_
