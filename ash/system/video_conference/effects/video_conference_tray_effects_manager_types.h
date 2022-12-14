// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

// All the data that's needed to present one possible state of a video
// conference effect UI control that's being hosted by a `VcEffectsDelegate`.
class ASH_EXPORT VcEffectState {
 public:
  // Use this in cases where an ID needs to be specified but isn't actually
  // used.
  static const int kUnusedId;

  // Arguments:
  //
  // `icon` - The icon displayed, used for all effect types (if non-nullptr).
  //
  // `label_text` - The text displayed.
  //
  // `accessible_name_id` - The ID of the string spoken when focused in a11y
  // mode.
  //
  // `button_callback` - A callback that's invoked when the user sets the effect
  // to this state.
  VcEffectState(const gfx::VectorIcon* icon,
                const std::u16string& label_text,
                int accessible_name_id,
                views::Button::PressedCallback button_callback);

  VcEffectState(const VcEffectState&) = delete;
  VcEffectState& operator=(const VcEffectState&) = delete;

  ~VcEffectState();

  const gfx::VectorIcon* icon() const { return icon_; }
  const std::u16string& label_text() const { return label_text_; }
  int accessible_name_id() const { return accessible_name_id_; }
  const views::Button::PressedCallback& button_callback() const {
    return button_callback_;
  }

 private:
  // The icon to be displayed.
  gfx::VectorIcon const* icon_;

  // The text to be displayed.
  std::u16string label_text_;

  // The ID of the string to be spoken, when this value is focused in
  // accessibility mode.
  int accessible_name_id_;

  // Callback that's bound to the delegate's `OnEffectActivated` function,
  // with the effect's ID and the actual (integer) value (e.g.
  // kBackgroundBlurMedium) member as arguments.
  views::Button::PressedCallback button_callback_;
};

// Designates the type of user-adjustments made to this effect.
enum class VcEffectType {
  // An effect that can only be set to on or off.
  kToggle = 0,

  // An effect that can be set to one of several integer values.
  kSetValue = 1,
};

// Represents a single video conference effect that's being "hosted" by an
// implementer of the `VcEffectsDelegate` interface, used to construct the
// effect's UI and perform any action that's needed to change the state of the
// effect.
class ASH_EXPORT VcHostedEffect {
 public:
  // The concept of "value" is not meaningful for `kToggle` effects, which
  // deal in a "state".
  enum ToggleState {
    kOff = 0,
    kOn = 1,
  };

  // `type` is the type of value adjustment allowed.
  explicit VcHostedEffect(VcEffectType type);

  VcHostedEffect(const VcHostedEffect&) = delete;
  VcHostedEffect& operator=(const VcHostedEffect&) = delete;

  ~VcHostedEffect();

  // Inserts `state` into the vector of allowable states for this effect.
  void AddState(std::unique_ptr<VcEffectState> state);

  // Retrieves the number of states.
  int GetNumStates() const;

  // Retrieves a raw pointer to the `VcEffectState` at `index`.
  const VcEffectState* GetState(int index) const;

  VcEffectType type() const { return type_; }
  void set_id(int id) { id_ = id; }
  int id() const { return id_; }
  void set_label_text(const std::u16string label_text) {
    label_text_ = label_text;
  }
  const std::u16string& label_text() const { return label_text_; }

 private:
  // Unique ID of the effect, set to `kDefaultId` in the absence of a
  // user-supplied ID.
  VcEffectType type_;

  // Unique ID of the effect, if desired.
  int id_;

  // Label text for the effect itself (that's separate from the label text of
  // individual child states).
  std::u16string label_text_;

  // Collection of possible effect states. All effects will have at least one.
  // `VcEffectState`s are constructed by `VcEffectsDelegate` subclasses (that
  // own the effects), and owned by the `VcHostedEffect` itself.
  std::vector<std::unique_ptr<VcEffectState>> states_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_