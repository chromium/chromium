// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace arc {
namespace input_overlay {

// Log events for debugging.
void LogEvent(const ui::Event& event);
void LogTouchEvents(const std::list<ui::TouchEvent>& events);
// TODO(cuicuiruan): Currently, it shows the dom_code.
// Will replace it with showing the result of dom_key / keyboard key depending
// on different keyboard layout.
std::string GetDisplayText(const std::string& dom_code_string);

// This is the base touch action which converts other events to touch
// events for input overlay.
class Action {
 public:
  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  virtual ~Action();

  virtual bool ParseFromJson(const base::Value& value);
  // 1. Return true & non-empty touch_events:
  //    Call SendEventFinally to send simulated touch event.
  // 2. Return true & empty touch_events:
  //    Call DiscardEvent to discard event such as repeated key event.
  // 3. Return false:
  //    No need to rewrite the event, so call SendEvent with original event.
  // |content_bounds| is the window bounds excluding caption.
  virtual bool RewriteEvent(const ui::Event& origin,
                            std::list<ui::TouchEvent>& touch_events,
                            const gfx::RectF& content_bounds) = 0;
  // Get the UI location in the content view.
  virtual gfx::PointF GetUIPosition(const gfx::RectF& content_bounds) = 0;
  virtual std::unique_ptr<ActionLabel> CreateView(
      const gfx::RectF& content_bounds) = 0;

  const std::string& name() { return name_; }
  const std::vector<std::unique_ptr<Position>>& locations() const {
    return locations_;
  }
  const aura::Window* target_window() const { return target_window_; }
  int current_position_index() const { return current_position_index_; }
  const absl::optional<int> touch_id() const { return touch_id_; }

  // Cancel event when the focus is leave or window is destroyed and the touch
  // event is still not released.
  absl::optional<ui::TouchEvent> GetTouchCancelEvent();
  // TODO (b/200210666): Can remove this after the bug is fixed.
  bool IsKeyAlreadyPressed(ui::DomCode code) const;

 protected:
  explicit Action(aura::Window* window);

  absl::optional<gfx::PointF> CalculateTouchPosition(
      const gfx::RectF& content_bounds);
  bool IsRepeatedKeyEvent(const ui::KeyEvent& key_event);
  void OnTouchReleased();
  void OnTouchCancelled();

  // name_ is basically for debugging and not visible to users.
  std::string name_;
  // Location take turns for each key press if there are more than
  // one location.
  std::vector<std::unique_ptr<Position>> locations_;

  aura::Window* target_window_;
  absl::optional<int> touch_id_;
  size_t current_position_index_ = 0;
  bool registered_ = false;

  gfx::PointF last_touch_root_location_;
  base::flat_set<ui::DomCode> keys_pressed_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
