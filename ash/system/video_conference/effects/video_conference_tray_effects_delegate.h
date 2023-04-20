// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_DELEGATE_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_DELEGATE_H_

#include <map>
#include <string>
#include <vector>

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

namespace ash {

enum class VcEffectId;

// An interface for hosting video conference effects, that are adjustable by the
// user via the video conference bubble. Subclasses must register with
// `VideoConferenceTrayEffectsManager`. At bubble construction time,
// `VideoConferenceTrayEffectsManager`'s vector of `VcEffectsDelegate` objects
// is used to construct the individual buttons and other value-adjustment
// controls needed for each effect.

// A `VcEffectsDelegate` is, at heart, a collection of effects and callbacks
// invoked when the user sets or the UI needs the effect value. Each effect is
// in turn a collection of values the user can set. It is intended to be
// flexible enough to accommodate a range of effect-hosting scenarios, from a
// single togglable effect to multiple togglable and integer set-value effects.
// TODO(b/274179052): Split the delegate into two separate classes for togglable
// and set-value effects.
class ASH_EXPORT VcEffectsDelegate {
 public:
  VcEffectsDelegate();

  VcEffectsDelegate(const VcEffectsDelegate&) = delete;
  VcEffectsDelegate& operator=(const VcEffectsDelegate&) = delete;

  virtual ~VcEffectsDelegate();

  // Inserts `effect` into the collection of effects hosted by this delegate.
  void AddEffect(std::unique_ptr<VcHostedEffect> effect);

  // Removes effect associated with `effect_id`.
  void RemoveEffect(VcEffectId effect_id);

  // Returns the number of hosted effects.
  int GetNumEffects();

  // Retrieves the `VcHostedEffect` given its `effect_id`.
  const VcHostedEffect* GetEffectById(VcEffectId effect_id);

  // Retrieves a std::vector<> of hosted effects of the passed-in `type`.
  std::vector<VcHostedEffect*> GetEffects(VcEffectType type);

  // Invoked when the UI controls are being constructed, to get the actual
  // effect state. `effect_id` specifies the effect whose state is requested,
  // and can be ignored if only one effect is being hosted. If no state can be
  // determined for `effect_id`, this function should return `absl::nullopt`.
  virtual absl::optional<int> GetEffectState(VcEffectId effect_id) = 0;

  // Invoked anytime the user makes an adjustment to an effect state. For
  // delegates that host more than a single effect, `effect_id` is the unique ID
  // of the activated effect. If only one effect is hosted, `effect_id` is
  // ignored and `absl::nullopt` should be passed. Similarly, `state` should be
  // `absl::nullopt` in cases (like toggle effects) where no specific state is
  // being set, an integer value otherwise.
  virtual void OnEffectControlActivated(VcEffectId effect_id,
                                        absl::optional<int> state) = 0;

  // This function will only be used for set-value effects, not for togglable
  // effects. Invoked when the user chooses a new value for the set-value effect
  // to record metrics. Note that for togglable effects, we are already
  // recording metrics in `ToggleEffectsView`, so no need further metrics
  // collection needed for them.
  virtual void RecordMetricsForSetValueEffect(VcEffectId effect_id,
                                              int state_value) const {}

 private:
  // Stores the collection of effects that are hosted by this delegate. The keys
  // are the unique ids of the effects.
  std::map<VcEffectId, std::unique_ptr<VcHostedEffect>> effects_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_DELEGATE_H_