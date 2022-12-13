// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"

namespace ash {

class VcEffectsDelegate;
class VcHostedEffect;

// The interface used to construct the UI that exposes video
// conferencing camera/microphone effects to the user.
class ASH_EXPORT VideoConferenceTrayEffectsManager {
 public:
  VideoConferenceTrayEffectsManager();

  VideoConferenceTrayEffectsManager(const VideoConferenceTrayEffectsManager&) =
      delete;
  VideoConferenceTrayEffectsManager& operator=(
      const VideoConferenceTrayEffectsManager&) = delete;

  virtual ~VideoConferenceTrayEffectsManager();

  // Register/unregister a delegate that hosts one or more effects.
  void RegisterDelegate(VcEffectsDelegate* delegate);
  void UnregisterDelegate(VcEffectsDelegate* delegate);

  // A vector (or row) of `VcHostedEffect` objects of type
  // `VcEffectType::kToggle`.
  using EffectDataVector = std::vector<const VcHostedEffect*>;

  // A table of `VcHostedEffect` objects, intended to
  // represent the arrangement of toggle effect buttons in the bubble.
  using EffectDataTable = std::vector<EffectDataVector>;

  // Returns 'true' if there are any `VcHostedEffect`
  // objects of type `VcEffectType::kToggle`, 'false'
  // otherwise.
  bool HasToggleEffects();

  // Returns a pre-arranged table of `VcEffectsDelegate`
  // objects.
  EffectDataTable GetToggleEffectButtonTable();

  // Returns 'true' if there are any `VcHostedEffect`
  // objects of type `VcEffectType::kSetValue`, 'false'
  // otherwise.
  bool HasSetValueEffects();

  // Returns a vector of `VcHostedEffect` objects of type
  // `VcEffectType::kSetValue`, in no special order.
  EffectDataVector GetSetValueEffects();

 private:
  // Returns a vector of `VcHostedEffect` objects of type
  // `VcEffectType::kToggle`, in no special order.
  EffectDataVector GetTotalToggleEffectButtons();

  // This list of registered effect delegates, unowned.
  std::vector<VcEffectsDelegate*> effect_delegates_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_EFFECTS_VIDEO_CONFERENCE_TRAY_EFFECTS_MANAGER_H_
