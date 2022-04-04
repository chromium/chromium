// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
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

// Return true if the |input_element| is bound.
bool IsBound(const InputElement& input_element);
// Return true if the |input_element| is bound to keyboard key.
bool IsKeyboardBound(const InputElement& input_element);
// Return true if the |input_element| is bound to mouse.
bool IsMouseBound(const InputElement& input_element);

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
  // Return false if |input_element| can take any binding elements from current
  // displayed binding. Return true if |input_element| can't take any binding
  // elements from current displayed binding.
  virtual bool RequireInputElement(const InputElement& input_element,
                                   Action** overlapped_action) = 0;
  // This is called if other action takes the input binding.
  virtual void Unbind() = 0;

  // This is called for editing the actions before change is saved.
  void PrepareToBind(std::unique_ptr<InputElement> input_element);
  // Save |pending_binding_| as |current_binding_|.
  void BindPending();
  // Cancel |pending_binding_|.
  void CancelPendingBind(const gfx::RectF& content_bounds);

  // Restore the input binding back to the original binding.
  void RestoreToDefault(const gfx::RectF& content_bounds);
  // Return currently displayed input binding.
  const InputElement& GetCurrentDisplayedBinding();

  InputElement* current_binding() const { return current_binding_.get(); }
  InputElement* original_binding() const { return original_binding_.get(); }
  InputElement* pending_binding() const { return pending_binding_.get(); }
  void set_pending_binding(std::unique_ptr<InputElement> binding) {
    if (pending_binding_)
      pending_binding_.reset();
    pending_binding_ = std::move(binding);
  }
  const std::string& name() { return name_; }
  const std::vector<std::unique_ptr<Position>>& locations() const {
    return locations_;
  }
  bool require_mouse_locked() const { return require_mouse_locked_; }
  aura::Window* target_window() const { return target_window_; }
  int current_position_index() const { return current_position_index_; }
  const absl::optional<int> touch_id() const { return touch_id_; }
  bool on_left_or_middle_side() const { return on_left_or_middle_side_; }
  bool support_modifier_key() const { return support_modifier_key_; }
  ActionView* action_view() const { return action_view_; }

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
  // Pending input binding. It is used during the editing before it is saved.
  std::unique_ptr<InputElement> pending_binding_;

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
  absl::optional<int> touch_id_;
  size_t current_position_index_ = 0;
  raw_ptr<aura::Window> target_window_;

  gfx::PointF last_touch_root_location_;
  base::flat_set<ui::DomCode> keys_pressed_;
  // This is used for marking the position of the UI view for the action.
  // According to the design spec, the label position depends
  // on whether the action position is on left or right.
  bool on_left_or_middle_side_ = false;
  absl::optional<float> radius_;
  // By default, it doesn't support modifier key.
  bool support_modifier_key_ = false;
  raw_ptr<ActionView> action_view_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
