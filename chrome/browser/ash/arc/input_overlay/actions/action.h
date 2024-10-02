// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_

#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/actions/position.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/point_f.h"

namespace arc::input_overlay {

constexpr char kKeyboard[] = "keyboard";
constexpr char kMouse[] = "mouse";

class ActionView;
class DisplayOverlayController;
class TouchInjector;

// Parse position from Json.
std::unique_ptr<Position> ParsePosition(const base::Value::Dict& dict);
// Log events for debugging.
void LogEvent(const ui::Event& event);
void LogTouchEvents(const std::list<ui::TouchEvent>& events);
// Json format:
// {
//    "key": "KeyA",
//    "modifiers": [""] // optional: "ctrl", "shift", "alt".
// }
std::optional<std::pair<ui::DomCode, int>> ParseKeyboardKey(
    const base::Value::Dict& value,
    std::string_view key_name);

// Return true if the `input_element` is bound.
bool IsInputBound(const InputElement& input_element);
// Return true if the `input_element` is bound to keyboard key.
bool IsKeyboardBound(const InputElement& input_element);
// Return true if the `input_element` is bound to mouse.
bool IsMouseBound(const InputElement& input_element);

// This is the base touch action which converts other events to touch
// events for input overlay.
class Action {
 public:
  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  virtual ~Action();

  virtual bool ParseFromJson(const base::Value::Dict& value);
  // Used to create an action from UI.
  virtual bool InitByAddingNewAction(const gfx::Point& target_pos);
  virtual void InitByChangingActionType(Action* action);

  bool ParseUserAddedActionFromProto(const ActionProto& proto);
  void OverwriteDefaultActionFromProto(const ActionProto& proto);
  // 1. Return true & non-empty touch_events:
  //    Call SendEventFinally to send simulated touch event.
  // 2. Return true & empty touch_events:
  //    Call DiscardEvent to discard event such as repeated key event.
  // 3. Return false:
  //    No need to rewrite the event, so call SendEvent with original event.
  virtual bool RewriteEvent(const ui::Event& origin,
                            const bool is_mouse_locked,
                            const gfx::Transform* rotation_transform,
                            std::list<ui::TouchEvent>& touch_events,
                            bool& keep_original_event) = 0;
  // Get the UI location in the content view.
  virtual gfx::PointF GetUICenterPosition() = 0;
  virtual std::unique_ptr<ActionView> CreateView(
      DisplayOverlayController* display_overlay_controller) = 0;
  // This is called if other actions take the input binding from this action.
  // `input_element` should overlap the current displayed binding. If it is
  // partially overlapped, then we only unbind the overlapped input.
  virtual void UnbindInput(const InputElement& input_element) = 0;
  virtual ActionType GetType() const = 0;

  // This is called for editing the actions before change is saved. Or for
  // loading the customized data to override the default input mapping.
  void PrepareToBindInput(std::unique_ptr<InputElement> input_element);
  // Save `pending_input_` as `current_input_`.
  void BindPending();
  // Cancel `pending_input_` and `pending_position_`.
  void CancelPendingBind();
  void ResetPendingBind();

  void PrepareToBindPosition(const gfx::Point& new_touch_center);

  // Restore the input binding back to the original binding.
  void RestoreToDefault();
  // Return currently displayed input binding.
  const InputElement& GetCurrentDisplayedInput();
  // Check if there is any overlap between `input_element` and current
  // displayed binding.
  bool IsOverlapped(const InputElement& input_element);
  // Make sure `original_positions_` is not empty before calling this.
  const Position& GetCurrentDisplayedPosition();

  // Return the proto object if the action is customized.
  virtual std::unique_ptr<ActionProto> ConvertToProtoIfCustomized() const;
  // Update `touch_down_positions_` after reposition, rotation change or window
  // bounds change.
  void UpdateTouchDownPositions();

  // Cancel event when the focus is lost or window is destroyed and the touch
  // event is still not released.
  std::optional<ui::TouchEvent> GetTouchCanceledEvent();
  std::optional<ui::TouchEvent> GetTouchReleasedEvent();
  int GetUIRadius();

  bool IsDefaultAction() const;

  // For default action, bind `current_input_` to nothing to indicate the
  // default action is deleted. For non-default action, the instance is removed
  // from list.
  void RemoveDefaultAction();
  // For default action, it is marked as deleted. For user action, the class
  // instance is deleted.
  bool IsDeleted();

  // Returns true if this action has event translated to touch events and not
  // released yet.
  bool IsActive();

  InputElement* current_input() const { return current_input_.get(); }
  InputElement* original_input() const { return original_input_.get(); }
  InputElement* pending_input() const { return pending_input_.get(); }
  void set_pending_input(std::unique_ptr<InputElement> input) {
    if (pending_input_)
      pending_input_.reset();
    pending_input_ = std::move(input);
  }
  int id() { return id_; }
  const std::string& name() { return name_; }
  const std::vector<Position>& original_positions() const {
    return original_positions_;
  }
  const std::vector<Position>& current_positions() const {
    return current_positions_;
  }
  const std::vector<gfx::PointF>& touch_down_positions() const {
    return touch_down_positions_;
  }
  std::optional<ActionType> original_type() { return original_type_; }
  void set_original_type(ActionType type) {
    original_type_ = std::make_optional<ActionType>(type);
  }
  bool require_mouse_locked() const { return require_mouse_locked_; }
  TouchInjector* touch_injector() const { return touch_injector_; }
  int current_position_idx() const { return current_position_idx_; }
  const std::optional<int> touch_id() const { return touch_id_; }
  bool on_left_or_middle_side() const { return on_left_or_middle_side_; }
  bool support_modifier_key() const { return support_modifier_key_; }
  ActionView* action_view() const { return action_view_; }
  void set_action_view(ActionView* action_view) { action_view_ = action_view; }
  int name_label_index() { return name_label_index_; }

  bool is_new() const { return is_new_; }
  void set_is_new(bool is_new) { is_new_ = is_new; }

 protected:
  // `touch_injector` must be non-NULL and own this Action.
  explicit Action(TouchInjector* touch_injector);

  // Create a touch pressed/moved/released event with `time_stamp` and save it
  // in `touch_events`.
  bool CreateTouchPressedEvent(const base::TimeTicks& time_stamp,
                               std::list<ui::TouchEvent>& touch_events);
  void CreateTouchMovedEvent(const base::TimeTicks& time_stamp,
                             std::list<ui::TouchEvent>& touch_events);
  void CreateTouchReleasedEvent(const base::TimeTicks& time_stamp,
                                std::list<ui::TouchEvent>& touch_events);

  bool IsRepeatedKeyEvent(const ui::KeyEvent& key_event);
  // Verify the key release event. If it is verified, it continues to simulate
  // the touch event. Otherwise, consider it as discard.
  bool VerifyOnKeyRelease(ui::DomCode code);
  // Process after unbinding the input mapping.
  void PostUnbindInputProcess();

  // Original input binding.
  std::unique_ptr<InputElement> original_input_;
  // Current input binding.
  std::unique_ptr<InputElement> current_input_;
  // Pending input binding. It is used during the editing before it is saved.
  // TODO(b/253646354): This will be removed when removing Beta flag.
  std::unique_ptr<InputElement> pending_input_;

  // Unique ID for each action.
  int id_ = 0;
  // Used for the default action.
  std::optional<ActionType> original_type_;
  // name_ is basically for debugging and not visible to users.
  std::string name_;
  // `name_label_index` is the index of the user-defined label for the action.
  // An negative index means that the action label name is unassigned.
  int name_label_index_ = -1;
  // Position take turns for each key press if there are more than
  // one positions. This is for original default positions.
  std::vector<Position> original_positions_;
  // The first element of `current_positions_` is different from
  // `original_positions_` if the position is customized.
  std::vector<Position> current_positions_;
  // Only support the reposition of the first touch position if there are more
  // than one touch position.
  // TODO(b/253646354): This will be removed when removing Beta flag.
  std::unique_ptr<Position> pending_position_;
  // Root locations of touch point.
  std::vector<gfx::PointF> touch_down_positions_;
  // If `require_mouse_locked_` == true, the action takes effect when the mouse
  // is locked. Once the mouse is unlocked, the active actions which need mouse
  // lock will be released.
  bool require_mouse_locked_ = false;
  bool is_new_ = false;
  int parsed_input_sources_ = 0;
  std::optional<int> touch_id_;
  size_t current_position_idx_ = 0;
  raw_ptr<TouchInjector> touch_injector_;

  gfx::PointF last_touch_root_location_;
  base::flat_set<ui::DomCode> keys_pressed_;
  // This is used for marking the position of the UI view for the action.
  // According to the design spec, the label position depends
  // on whether the action position is on left or right.
  bool on_left_or_middle_side_ = false;
  std::optional<float> radius_;
  // By default, it doesn't support modifier key.
  bool support_modifier_key_ = false;
  raw_ptr<ActionView, DanglingUntriaged> action_view_ = nullptr;

 private:
  friend class TouchInjectorTest;

  void OnTouchReleased();
  void OnTouchCancelled();
  // Create a touch event of `type` with `time_stamp` and save it
  // in `touch_events`.
  void CreateTouchEvent(ui::EventType type,
                        const base::TimeTicks& time_stamp,
                        std::list<ui::TouchEvent>& touch_events);

  void PrepareToBindPositionForTesting(std::unique_ptr<Position> position);
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_H_
