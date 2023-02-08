// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"

namespace ash {

namespace {

// Returns whether all resources, which `effect` depends on, are
// presented/enabled.
bool DependenciesSatisfied(VcHostedEffect* effect) {
  DCHECK(effect);

  const VcHostedEffect::ResourceDependencyFlags dependency_flags =
      effect->dependency_flags();
  VideoConferenceTrayController* controller =
      VideoConferenceTrayController::Get();
  if (dependency_flags & VcHostedEffect::ResourceDependency::kCamera &&
      controller->GetCameraMuted()) {
    // `effect` has a camera dependency and camera is muted.
    return false;
  }
  if (dependency_flags & VcHostedEffect::ResourceDependency::kMicrophone &&
      controller->GetMicrophoneMuted()) {
    // `effect` has a microphone dependency and microphone is muted.
    return false;
  }

  // All dependencies satisfied.
  return true;
}

}  // namespace

VcEffectsDelegate::VcEffectsDelegate() = default;

VcEffectsDelegate::~VcEffectsDelegate() = default;

void VcEffectsDelegate::AddEffect(std::unique_ptr<VcHostedEffect> effect) {
  effects_.push_back(std::move(effect));
}

int VcEffectsDelegate::GetNumEffects() {
  return effects_.size();
}

const VcHostedEffect* VcEffectsDelegate::GetEffect(int index) {
  DCHECK(index >= 0 && index < GetNumEffects());
  return effects_[index].get();
}

std::vector<VcHostedEffect*> VcEffectsDelegate::GetEffects(VcEffectType type) {
  std::vector<VcHostedEffect*> effects_of_type;

  for (auto& effect : effects_) {
    if (!DependenciesSatisfied(effect.get())) {
      // `effect` has at least one resource dependency that is not satisfied,
      // and is therefore not inserted in the output vector.
      continue;
    }

    if (effect->type() == type) {
      effects_of_type.push_back(effect.get());
    }
  }

  return effects_of_type;
}

}  // namespace ash