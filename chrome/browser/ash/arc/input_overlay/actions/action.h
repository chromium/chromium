// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace arc {
namespace input_overlay {

constexpr char kKeyboard[] = "keyboard";
constexpr char kMouse[] = "mouse";

class ActionView;
class DisplayOverlayController;

// Parse position from Json.
std::unique_ptr<Position> ParsePosition(const base::Value& value);
// Log events for debugging.
void LogEvent(const ui::Event& event);
void LogTouchEvents(const std::list<ui::TouchEvent>& events);
// Json format:
// {
//    "key": "KeyA",
//    "modifiers": [""] // optional: "ctrl", "shift", "alt".
// }
absl::optional<std::pair<ui::DomCode, int>> ParseKeyboardKey(
    const base::Value& value,
    const base::StringPiece key_name);

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
                            const gfx::RectF& content_bounds,
                            const bool is_mouse_locked,
                            std::list<ui::TouchEvent>& touch_events,
                            bool& keep_original_event) = 0;
  // Get the UI location in the content view.
  virtual gfx::PointF GetUICenterPosition(const gfx::RectF& content_bounds) = 0;
  virtual std::unique_ptr<ActionView> CreateView(
      DisplayOverlayController* display_overlay_controller,
      const gfx::RectF& content_bounds) = 0;

  bool IsNoneBound();
  bool IsKeyboardBound();
  bool IsMouseBound();

  InputElement* current_binding() const { return current_binding_.get(); }
  const std::string& name() { return name_; }
  const std::vector<std::unique_ptr<Position>>& locations() const {
    return locations_;
  }
  bool require_mouse_locked() const { return require_mouse_locked_; }
  const aura::Window* target_window() const { return target_window_; }
  int current_position_index() const { return current_position_index_; }
  const absl::optional<int> touch_id() const { return touch_id_; }
  bool on_left_or_middle_side() const { return on_left_or_middle_side_; }

  // Cancel event when the focus is leave or window is destroyed and the touch
  // event is still not released.
  absl::optional<ui::TouchEvent> GetTouchCanceledEvent();
  absl::optional<ui::TouchEvent> GetTouchReleasedEvent();
  int GetUIRadius(const gfx::RectF& content_bounds);

 protected:
  explicit Action(aura::Window* window);

  absl::optional<gfx::PointF> CalculateTouchPosition(
      const gfx::RectF& content_bounds);
  bool IsRepeatedKeyEvent(const ui::KeyEvent& key_event);
  void OnTouchReleased();
  void OnTouchCancelled();

  // Original input binding.
  std::unique_ptr<InputElement> original_binding_;
  // Current input binding.
  std::unique_ptr<InputElement> current_binding_;

  // name_ is basically for debugging and not visible to users.
  std::string name_;
  // Location take turns for each key press if there are more than
  // one location.
  std::vector<std::unique_ptr<Position>> locations_;
  // If |require_mouse_locked_| == true, the action takes effect when the mouse
  // is locked. Once the mouse is unlocked, the active actions which need mouse
  // lock will be released.
  bool require_mouse_locked_ = false;
  int parsed_input_sources_ = 0;
  aura::Window* target_window_;
  absl::optional<int> touch_id_;
  size_t current_position_index_ = 0;

  gfx::PointF last_touch_root_location_;
  base::flat_set<ui::DomCode> keys_pressed_;
  // This is used for marking the position of the UI view for the action.
  // According to the design spec, the label position depends
  // on whether the action position is on left or right.
  bool on_left_or_middle_side_ = false;
  absl::optional<float> radius_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
