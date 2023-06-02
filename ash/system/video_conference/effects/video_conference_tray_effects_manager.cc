// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"

#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/cxx20_erase_vector.h"
#include "base/observer_list.h"

namespace ash {

VideoConferenceTrayEffectsManager::VideoConferenceTrayEffectsManager() =
    default;

VideoConferenceTrayEffectsManager::~VideoConferenceTrayEffectsManager() =
    default;

void VideoConferenceTrayEffectsManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VideoConferenceTrayEffectsManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void VideoConferenceTrayEffectsManager::RegisterDelegate(
    VcEffectsDelegate* delegate) {
  DCHECK(delegate);
  DCHECK(!IsDelegateRegistered(delegate));
  effect_delegates_.push_back(delegate);
}

void VideoConferenceTrayEffectsManager::UnregisterDelegate(
    VcEffectsDelegate* delegate) {
  DCHECK(delegate);
  size_t num_items_erased =
      base::EraseIf(effect_delegates_,
                    [delegate](VcEffectsDelegate* d) { return delegate == d; });
  DCHECK_EQ(num_items_erased, 1UL);
}

bool VideoConferenceTrayEffectsManager::IsDelegateRegistered(
    VcEffectsDelegate* delegate) {
  DCHECK(delegate);
  return std::find_if(effect_delegates_.begin(), effect_delegates_.end(),
                      [delegate](VcEffectsDelegate* d) {
                        return delegate == d;
                      }) != effect_delegates_.end();
}

bool VideoConferenceTrayEffectsManager::HasToggleEffects() {
  return GetTotalToggleEffectButtons().size() > 0;
}

VideoConferenceTrayEffectsManager::EffectDataTable
VideoConferenceTrayEffectsManager::GetToggleEffectButtonTable() {
  EffectDataVector total_buttons = GetTotalToggleEffectButtons();

  EffectDataVector row;
  EffectDataTable buttons;

  int num_buttons = total_buttons.size();
  if (num_buttons == 0) {
    return buttons;
  }

  if (num_buttons <= 3) {
    // For 3 or fewer, `effects_buttons` is the entire row.
    row = total_buttons;
    buttons.push_back(row);
    return buttons;
  }

  // Max of 2 per row.
  for (int i = 0; i < num_buttons; ++i) {
    row.push_back(total_buttons[i]);
    if (i % 2 == 1 || i == num_buttons - 1) {
      buttons.push_back(row);
      row.clear();
    }
  }

  return buttons;
}

bool VideoConferenceTrayEffectsManager::HasSetValueEffects() {
  return GetSetValueEffects().size() > 0;
}

VideoConferenceTrayEffectsManager::EffectDataVector
VideoConferenceTrayEffectsManager::GetSetValueEffects() {
  EffectDataVector effects;

  for (auto* delegate : effect_delegates_) {
    for (auto* effect : delegate->GetEffects(VcEffectType::kSetValue)) {
      effects.push_back(effect);
    }
  }

  return effects;
}

void VideoConferenceTrayEffectsManager::NotifyEffectSupportStateChanged(
    VcEffectId effect_id,
    bool is_supported) {
  for (auto& observer : observers_) {
    observer.OnEffectSupportStateChanged(effect_id, is_supported);
  }
}

void VideoConferenceTrayEffectsManager::RecordInitialStates() {
  for (auto* delegate : effect_delegates_) {
    delegate->RecordInitialStates();
  }
}

VideoConferenceTrayEffectsManager::EffectDataVector
VideoConferenceTrayEffectsManager::GetTotalToggleEffectButtons() {
  EffectDataVector effects;

  for (auto* delegate : effect_delegates_) {
    for (auto* effect : delegate->GetEffects(VcEffectType::kToggle)) {
      effects.push_back(effect);
    }
  }

  return effects;
}

}  // namespace ash
