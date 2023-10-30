// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}

namespace ash {

class VcEffectsDelegate;

// All the data that's needed to present one possible state of a video
// conference effect UI control that's being hosted by a `VcEffectsDelegate`.
class ASH_EXPORT VcEffectState {
 public:
  // Arguments:
  //
  // `icon` - The icon displayed.
  //
  // `label_text` - The text displayed.
  //
  // `accessible_name_id` - The ID of the string spoken when focused in a11y
  //                        mode.
  //
  // `button_callback` - A callback that's invoked when the user sets the effect
  //                     to this state.
  //
  // `state` - The actual state value. Optional because only certain types of
  //           effects (e.g. set-value) actually need it.
  VcEffectState(const gfx::VectorIcon* icon,
                const std::u16string& label_text,
                int accessible_name_id,
                views::Button::PressedCallback button_callback,
                absl::optional<int> state_value = absl::nullopt);

  VcEffectState(const VcEffectState&) = delete;
  VcEffectState& operator=(const VcEffectState&) = delete;

  ~VcEffectState();

  absl::optional<int> state_value() const { return state_value_; }
  const gfx::VectorIcon* icon() const { return icon_; }
  const std::u16string& label_text() const { return label_text_; }
  int accessible_name_id() const { return accessible_name_id_; }
  const views::Button::PressedCallback& button_callback() const {
    return button_callback_;
  }

 private:
  // The icon to be displayed.
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon_;

  // The text to be displayed.
  std::u16string label_text_;

  // The ID of the string to be spoken, when this value is focused in
  // accessibility mode.
  int accessible_name_id_;

  // Callback that's bound to the delegate's `OnEffectActivated` function,
  // with the effect's ID and the actual (integer) value (e.g.
  // kBackgroundBlurMedium) member as arguments.
  views::Button::PressedCallback button_callback_;

  // The state value.
  absl::optional<int> state_value_;
};

// Designates the type of user-adjustments made to this effect.
enum class VcEffectType {
  // An effect that can only be set to on or off.
  kToggle = 0,

  // An effect that can be set to one of several integer values.
  kSetValue = 1,
};

// Represents all the available effects in the Video Conference panel. Each
// effect must have its own id for the purpose of metrics collection, unless it
// is for testing.
enum class VcEffectId {
  kTestEffect = -1,
  kBackgroundBlur = 0,
  kPortraitRelighting = 1,
  kNoiseCancellation = 2,
  kLiveCaption = 3,
  kCameraFraming = 4,
  kMaxValue = kCameraFraming,
};

// Represents a single video conference effect that's being "hosted" by an
// implementer of the `VcEffectsDelegate` interface, used to construct the
// effect's UI and perform any action that's needed to change the state of the
// effect.
class ASH_EXPORT VcHostedEffect {
 public:
  // Each of these denotes a resource dependency for an effect e.g.
  // `kMicrophone` is a dependency of mic noise cancellation. An effect can have
  // more than one dependency, which is why these are bitfields.
  enum ResourceDependency {
    kNone = 0,
    kCamera = 1 << 0,
    kMicrophone = 1 << 1,
  };

  // All of a `VcHostedEffect`'s resource dependencies.
  using ResourceDependencyFlags = int32_t;

  // Callback for obtaining the current state of the effect. The callback must
  // have the effect ID bound as an argument.
  using GetEffectStateCallback =
      base::RepeatingCallback<absl::optional<int>(void)>;

  // `type` is the type of value adjustment allowed.
  // `get_state_callback` is invoked to obtain the current state of the effect.
  VcHostedEffect(VcEffectType type,
                 GetEffectStateCallback get_state_callback,
                 VcEffectId effect_id);

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

  const GetEffectStateCallback& get_state_callback() const {
    return get_state_callback_;
  }

  VcEffectId id() const { return id_; }

  void set_label_text(const std::u16string label_text) {
    label_text_ = label_text;
  }
  const std::u16string& label_text() const { return label_text_; }

  void set_dependency_flags(ResourceDependencyFlags dependency_flags) {
    dependency_flags_ = dependency_flags;
  }
  ResourceDependencyFlags dependency_flags() const { return dependency_flags_; }

  void set_effects_delegate(VcEffectsDelegate* delegate) {
    delegate_ = delegate;
  }
  VcEffectsDelegate* delegate() const { return delegate_; }

  absl::optional<int> container_id() const { return container_id_; }
  void set_container_id(absl::optional<int> id) { container_id_ = id; }

 private:
  // The type of value adjustment of this effect,
  VcEffectType type_;

  // The set of resources needed in order for the effect's controls to be
  // presented to the user.
  ResourceDependencyFlags dependency_flags_ = ResourceDependency::kNone;

  // Callback supplied by the parent `VcEffectsDelegate`, for obtaining the
  // state of the effect.
  GetEffectStateCallback get_state_callback_;

  // Unique ID of the effect.
  const VcEffectId id_;

  // Label text for the effect (that's separate from the label text of
  // individual child states).
  std::u16string label_text_;

  // Collection of possible effect states. All effects will have at least one.
  // `VcEffectState`s are constructed by `VcEffectsDelegate` subclasses (that
  // own the effects), and owned by the `VcHostedEffect` itself.
  std::vector<std::unique_ptr<VcEffectState>> states_;

  // Optional ID assigned to the outermost container view for this effect's
  // toggle control. For testing only, this facilitates easy lookup of the
  // outermost container that houses the UI controls for this effect.
  absl::optional<int> container_id_;

  // The effects delegate associated with this effect.
  raw_ptr<VcEffectsDelegate, ExperimentalAsh> delegate_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_TYPES_H_