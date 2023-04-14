// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

VcEffectState::VcEffectState(const gfx::VectorIcon* enabled_icon,
                             const std::u16string& label_text,
                             int accessible_name_id,
                             views::Button::PressedCallback button_callback,
                             absl::optional<int> state_value)
    : enabled_icon_(enabled_icon),
      label_text_(label_text),
      accessible_name_id_(accessible_name_id),
      button_callback_(button_callback),
      state_value_(state_value) {
  DCHECK(enabled_icon);
}

VcEffectState::~VcEffectState() = default;

VcHostedEffect::VcHostedEffect(VcEffectType type,
                               GetEffectStateCallback get_state_callback,
                               VcEffectId effect_id)
    : type_(type), get_state_callback_(get_state_callback), id_(effect_id) {}

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