// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_INPUT_ELEMENT_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_INPUT_ELEMENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"

namespace arc::input_overlay {

// About Json strings.
constexpr char kMouseAction[] = "mouse_action";
constexpr char kPrimaryClick[] = "primary_click";
constexpr char kSecondaryClick[] = "secondary_click";
constexpr char kHoverMove[] = "hover_move";
constexpr char kPrimaryDragMove[] = "primary_drag_move";
constexpr char kSecondaryDragMove[] = "secondary_drag_move";

// Total key size for ActionMoveKey.
constexpr size_t kActionMoveKeysSize = 4;
// Gets the event flags for the modifier domcode. Return ui::DomCode::NONE if
// |code| is not modifier DomCode.
int ModifierDomCodeToEventFlag(ui::DomCode code);
bool IsSameDomCode(ui::DomCode a, ui::DomCode b);
// Convert mouse action strings to enum values.
MouseAction ConvertToMouseActionEnum(const std::string& mouse_action);

// InputElement creates input elements bound for each action.
// TODO(cuicuiruan): It only supports ActionTap and ActionMove now. Supports
// more actions as needed.
class InputElement {
 public:
  InputElement();
  explicit InputElement(ui::DomCode code);
  InputElement(const InputElement& other);
  ~InputElement();

  // Create key binding for tap action.
  static std::unique_ptr<InputElement> CreateActionTapKeyElement(
      ui::DomCode key);
  // Create mouse binding for tap action.
  static std::unique_ptr<InputElement> CreateActionTapMouseElement(
      const std::string& mouse_action);
  // Create key binding for move action.
  static std::unique_ptr<InputElement> CreateActionMoveKeyElement(
      const std::vector<ui::DomCode>& keys);
  // Create mouse binding for move action.
  static std::unique_ptr<InputElement> CreateActionMoveMouseElement(
      const std::string& mouse_action);
  // Create input binding from Proto object.
  static std::unique_ptr<InputElement> ConvertFromProto(
      const InputElementProto& proto);

  // Return true if there is key overlapped or the mouse action is overlapped.
  bool IsOverlapped(const InputElement& input_element) const;
  // Set key in the |keys_| list at the |index| to |code|.
  void SetKey(size_t index, ui::DomCode code);
  // Set keys to |keys|.
  void SetKeys(std::vector<ui::DomCode>& keys);
  // If it is keyboard-binded input and there is |key| binded, return the index
  // of the |key|. Otherwise, return -1;
  int GetIndexOfKey(ui::DomCode key) const;
  std::unique_ptr<InputElementProto> ConvertToProto();

  int input_sources() const { return input_sources_; }
  void set_input_sources(int input_sources) { input_sources_ = input_sources; }
  const std::vector<ui::DomCode>& keys() const { return keys_; }
  bool is_modifier_key() { return is_modifier_key_; }
  MouseAction mouse_action() const { return mouse_action_; }
  const base::flat_set<ui::EventType>& mouse_types() const {
    return mouse_types_;
  }
  int mouse_flags() const { return mouse_flags_; }

  bool operator==(const InputElement& other) const;
  bool operator!=(const InputElement& other) const;

 private:
  // Input source for this input element, could be keyboard or mouse or both.
  int input_sources_ = InputSource::IS_NONE;

  // For key binding.
  std::vector<ui::DomCode> keys_;
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

  // For mouse binding.
  bool mouse_lock_required_ = false;
  // Tap action: PRIMARY_CLICK and SECONDARY_CLICK.
  // Move action: HOVER_MOVE, PRIMARY_DRAG_MOVE and SECONDARY_DRAG_MOVE.
  MouseAction mouse_action_ = MouseAction::NONE;
  // Tap action for mouse primary/secondary click: ET_MOUSE_PRESSED,
  // ET_MOUSE_RELEASED. Move action for primary/secondary drag move:
  // ET_MOUSE_PRESSED, ET_MOUSE_DRAGGED, ET_MOUSE_RELEASED.
  base::flat_set<ui::EventType> mouse_types_;
  // Mouse primary button flag: EF_LEFT_MOUSE_BUTTON. Secondary button flag:
  // EF_RIGHT_MOUSE_BUTTON.
  int mouse_flags_ = 0;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_INPUT_ELEMENT_H_
