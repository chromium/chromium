// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

// static
const int VcEffectState::kUnusedId = -1;

VcEffectState::VcEffectState(const gfx::VectorIcon* icon,
                             const std::u16string& label_text,
                             int accessible_name_id,
                             views::Button::PressedCallback button_callback)
    : icon_(icon),
      label_text_(label_text),
      accessible_name_id_(accessible_name_id),
      button_callback_(button_callback) {}

VcEffectState::~VcEffectState() = default;

VcHostedEffect::VcHostedEffect(VcEffectType type)
    : type_(type), id_(VcEffectState::kUnusedId) {}

VcHostedEffect::~VcHostedEffect() = default;

void VcHostedEffect::AddState(std::unique_ptr<VcEffectState> state) {
  states_.push_back(std::move(state));
}

int VcHostedEffect::GetNumStates() const {
  return states_.size();
}

const VcEffectState* VcHostedEffect::GetState(int index) const {
  DCHECK(index >= 0 && index < (int)states_.size());
  return states_[index].get();
}

}  // namespace ash