// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/fake_video_conference_tray_effects_manager.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

namespace ash {

FakeVideoConferenceTrayEffectsManager::FakeVideoConferenceTrayEffectsManager() =
    default;

FakeVideoConferenceTrayEffectsManager::
    ~FakeVideoConferenceTrayEffectsManager() = default;

void FakeVideoConferenceTrayEffectsManager::SetDlcIdsForEffectId(
    VcEffectId effect_id,
    std::vector<std::string> dlc_ids) {
  dlc_ids_for_effect_id_[effect_id] = std::move(dlc_ids);
}

video_conference::VcTileUiController*
FakeVideoConferenceTrayEffectsManager::GetUiControllerForEffectId(
    VcEffectId effect_id) {
  CHECK(features::IsVcDlcUiEnabled());
  auto toggle_effects = GetToggleEffects();
  if (toggle_effects_index_ >= toggle_effects.size()) {
    // If we're out of bounds try starting from the beginning again.
    toggle_effects_index_ = 0;
  }

  // Re-use an existing tile UI controller if we have one.
  if (toggle_effects_index_ < tile_ui_controllers_.size()) {
    return tile_ui_controllers_[toggle_effects_index_].get();
  }

  // Otherwise create a new tile UI controller and return that.
  auto* effect = toggle_effects[toggle_effects_index_++];
  tile_ui_controllers_.push_back(
      std::make_unique<video_conference::VcTileUiController>(effect));
  return tile_ui_controllers_.back().get();
}

std::vector<std::string>
FakeVideoConferenceTrayEffectsManager::GetDlcIdsForEffectId(
    VcEffectId effect_id) {
  if (base::Contains(dlc_ids_for_effect_id_, effect_id)) {
    return dlc_ids_for_effect_id_[effect_id];
  }
  return VideoConferenceTrayEffectsManager::GetDlcIdsForEffectId(effect_id);
}

void FakeVideoConferenceTrayEffectsManager::UnregisterDelegate(
    VcEffectsDelegate* delegate) {
  VideoConferenceTrayEffectsManager::UnregisterDelegate(delegate);

  // Delete any tile UI controllers associated with any of `delegate`'s effects.
  for (auto* effect : delegate->GetAllEffects(VcEffectType::kToggle)) {
    const VcEffectId id = effect->id();
    std::erase_if(
        tile_ui_controllers_,
        [&, id](const std::unique_ptr<video_conference::VcTileUiController>&
                    controller) {
          return controller->effect_id_for_testing() == id;
        });
  }
}

}  // namespace ash
