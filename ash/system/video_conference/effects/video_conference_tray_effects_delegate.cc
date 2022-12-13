// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

namespace ash {

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
    if (effect->type() == type)
      effects_of_type.push_back(effect.get());
  }

  return effects_of_type;
}

}  // namespace ash