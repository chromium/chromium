// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_effects_manager_types.h"

#include "ash/system/video_conference/video_conference_tray_effects_manager.h"

namespace ash {

// static
const int VcEffectState::kUnusedId = -1;

VcEffectState::VcEffectState(int value,
                             const gfx::VectorIcon* icon,
                             const std::u16string& label_text,
                             int accessible_name_id,
                             views::Button::PressedCallback button_callback)
    : value_(value),
      icon_(icon),
      label_text_(label_text),
      accessible_name_id_(accessible_name_id),
      button_callback_(button_callback) {}

VcEffectState::~VcEffectState() = default;

VcHostedEffect::VcHostedEffect(VcEffectType type)
    : type_(type), id_(VcEffectState::kUnusedId) {}

VcHostedEffect::~VcHostedEffect() = default;

void VcHostedEffect::AddState(const VcEffectState* value) {
  DCHECK(value);
  states_.push_back(value);
}

}  // namespace ash