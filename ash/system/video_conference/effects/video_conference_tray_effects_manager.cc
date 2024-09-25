// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/video_conference/bubble/vc_tile_ui_controller.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"

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
  // TODO(b/345831029): Test that a VcTileUiController is reset when an effect
  // is removed.
  if (features::IsVcDlcUiEnabled()) {
    delegate->set_on_effect_will_be_removed_callback(base::BindRepeating(
        &VideoConferenceTrayEffectsManager::RemoveTileControllers,
        weak_factory_.GetWeakPtr()));
  }
}

void VideoConferenceTrayEffectsManager::UnregisterDelegate(
    VcEffectsDelegate* delegate) {
  DCHECK(delegate);
  size_t num_items_erased =
      std::erase_if(effect_delegates_,
                    [delegate](VcEffectsDelegate* d) { return delegate == d; });
  DCHECK_EQ(num_items_erased, 1UL);

  if (features::IsVcDlcUiEnabled()) {
    // Remove tile controllers for effects hosted by the delegate being
    // unregistered.
    RemoveTileControllers(delegate);
  }
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

  for (ash::VcEffectsDelegate* delegate : effect_delegates_) {
    for (auto* effect :
         delegate->GetAvailableEffects(VcEffectType::kSetValue)) {
      effects.push_back(effect);
    }
  }

  return effects;
}

VideoConferenceTrayEffectsManager::EffectDataVector
VideoConferenceTrayEffectsManager::GetToggleEffects() {
  EffectDataVector effects;

  for (ash::VcEffectsDelegate* delegate : effect_delegates_) {
    for (auto* effect : delegate->GetAvailableEffects(VcEffectType::kToggle)) {
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

void VideoConferenceTrayEffectsManager::NotifyEffectChanged(
    VcEffectId effect_id,
    bool is_on) {
  for (auto& observer : observers_) {
    observer.OnEffectChanged(effect_id, is_on);
  }
}

void VideoConferenceTrayEffectsManager::NotifyVideoConferenceBubbleOpened() {
  for (auto& observer : observers_) {
    observer.OnVideoConferenceBubbleOpened();
  }
}

void VideoConferenceTrayEffectsManager::RecordInitialStates() {
  for (ash::VcEffectsDelegate* delegate : effect_delegates_) {
    delegate->RecordInitialStates();
  }
}

video_conference::VcTileUiController*
VideoConferenceTrayEffectsManager::GetUiControllerForEffectId(
    VcEffectId effect_id) {
  CHECK(features::IsVcDlcUiEnabled());
  if (!base::Contains(controller_for_effect_id_, effect_id)) {
    for (auto* effect : GetToggleEffects()) {
      if (effect_id == effect->id()) {
        controller_for_effect_id_[effect_id] =
            std::make_unique<video_conference::VcTileUiController>(effect);
        break;
      }
    }
  }
  return controller_for_effect_id_[effect_id].get();
}

std::vector<std::string>
VideoConferenceTrayEffectsManager::GetDlcIdsForEffectId(VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kLiveCaption: {
      PrefService* pref_service =
          Shell::Get()->session_controller()->GetActivePrefService();
      std::string locale = pref_service
                               ? prefs::GetLiveCaptionLanguageCode(pref_service)
                               : speech::kUsEnglishLocale;
      std::string dlc_name =
          speech::SodaInstaller::GetInstance()->GetLanguageDlcNameForLocale(
              locale);

      // Should always have a language DLC lib for a specific language.
      CHECK(!dlc_name.empty());

      // "Live caption" requires both a binary ("libsoda") as well as a specific
      // language model (e.g. "libsoda-model-en-us") to operate.
      return {"libsoda", dlc_name};
    }
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kFaceRetouch:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kStudioLook:
      return {"ml-core-internal"};
    case VcEffectId::kTestEffect:
    case VcEffectId::kNoiseCancellation:
    case VcEffectId::kStyleTransfer:
    case VcEffectId::kCameraFraming:
      return {};
  }
}

VideoConferenceTrayEffectsManager::EffectDataVector
VideoConferenceTrayEffectsManager::GetTotalToggleEffectButtons() {
  EffectDataVector effects;

  for (ash::VcEffectsDelegate* delegate : effect_delegates_) {
    for (auto* effect : delegate->GetAvailableEffects(VcEffectType::kToggle)) {
      effects.push_back(effect);
    }
  }

  std::sort(effects.begin(), effects.end(),
            [](const VcHostedEffect* lhs, const VcHostedEffect* rhs) {
              return lhs->id() < rhs->id();
            });
  return effects;
}

void VideoConferenceTrayEffectsManager::RemoveTileControllers(
    VcEffectsDelegate* delegate) {
  if (!features::IsVcDlcUiEnabled()) {
    return;
  }
  for (auto* effect : delegate->GetAllEffects(VcEffectType::kToggle)) {
    const VcEffectId id = effect->id();
    if (base::Contains(controller_for_effect_id_, id)) {
      controller_for_effect_id_.erase(id);
    }
  }
}

}  // namespace ash
