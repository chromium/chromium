// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/button.h"

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
  // `value` - For `kSetValue` effects, the actual integer value that represents
  // the effect being in this state e.g. background blur (the effect) set to
  // "low" (the state). This member is ignored for `kToggle` effects, which can
  // pass `kValueUnused` here.
  //
  // `icon` - The icon displayed, used for all effect types (if non-nullptr).
  //
  // `label_text` - The text displayed.
  //
  // `accessible_name_id` - The ID of the string spoken when focused in a11y
  // mode.
  VcEffectState(int value,
                const gfx::VectorIcon* icon,
                const std::u16string& label_text,
                int accessible_name_id,
                views::Button::PressedCallback button_callback);

  ~VcEffectState();

  int value() const { return value_; }
  const gfx::VectorIcon* icon() const { return icon_; }
  const std::u16string& label_text() const { return label_text_; }
  int accessible_name_id() const { return accessible_name_id_; }
  const views::Button::PressedCallback& button_callback() const {
    return button_callback_;
  }

 private:
  // The actual value of the effect-state, ignored for effects of type
  // `kToggle`.
  int value_;

  // The icon to be displayed.
  gfx::VectorIcon const* icon_;

  // The text to be displayed.
  std::u16string label_text_;

  // The ID of the string to be spoken, when this value is focused in
  // accessibility mode.
  int accessible_name_id_;

  // Callback that's bound to the delegate's `OnEffectActivated` function, with
  // the effect's ID and our `value_` member as arguments.
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
  // The concept of "value" is not meaningful for `kToggle` effects, which deal
  // in a "state".
  enum ToggleState {
    kOff = 0,
    kOn = 1,
  };

  // `type` is the type of value adjustment allowed.
  explicit VcHostedEffect(VcEffectType type);

  ~VcHostedEffect();

  // Inserts `value` into the vector of possible values for this effect.
  void AddState(const VcEffectState* value);

  VcEffectType type() const { return type_; }
  void set_id(int id) { id_ = id; }
  int id() const { return id_; }
  void set_label_text(const std::u16string label_text) {
    label_text_ = label_text;
  }
  const std::u16string& label_text() const { return label_text_; }
  const std::vector<const VcEffectState*>& states() const { return states_; }

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
  std::vector<const VcEffectState*> states_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_