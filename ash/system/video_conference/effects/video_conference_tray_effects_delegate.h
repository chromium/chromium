// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_DELEGATE_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

namespace ash {

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
class ASH_EXPORT VcEffectsDelegate {
 public:
  VcEffectsDelegate();

  VcEffectsDelegate(const VcEffectsDelegate&) = delete;
  VcEffectsDelegate& operator=(const VcEffectsDelegate&) = delete;

  virtual ~VcEffectsDelegate();

  // Inserts `effect` into the vector of effects hosted by this delegate.
  void AddEffect(std::unique_ptr<VcHostedEffect> effect);

  // Returns the number of hosted effects.
  int GetNumEffects();

  // Retrieves the `VcHostedEffect` at `index`.
  const VcHostedEffect* GetEffect(int index);

  // Retrieves a std::vector<> of hosted effects of the passed-in `type`.
  std::vector<VcHostedEffect*> GetEffects(VcEffectType type);

  // Invoked when the UI controls are being constructed, to get the actual
  // effect state. `effect_id` specifies the effect whose state is requested,
  // and can be ignored if only one effect is being hosted.
  virtual int GetEffectState(int effect_id) = 0;

  // Invoked anytime the user makes an adjustment. `effect_id` is the unique ID
  // of the hosted effect, and delegates that host only a single effect can
  // safely ignore it. `value` is the integer value the user just set, which can
  // be ignored if the effect is of type `kToggle`.
  virtual void OnEffectControlActivated(int effect_id, int value) = 0;

 private:
  // Effects are created by `VcEffectsDelegate` subclasses.
  std::vector<std::unique_ptr<VcHostedEffect>> effects_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_DELEGATE_H_