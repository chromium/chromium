// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/effects/video_conference_tray_effects_delegate.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "base/metrics/histogram_functions.h"

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
      !controller->HasCameraPermission()) {
    // `effect` has a camera dependency, but the apps do not have permission.
    return false;
  }
  if (dependency_flags & VcHostedEffect::ResourceDependency::kMicrophone &&
      !controller->HasMicrophonePermission()) {
    // `effect` has a microphone dependency, but the apps do not have
    // permission.
    return false;
  }

  // All dependencies satisfied.
  return true;
}

}  // namespace

VcEffectsDelegate::VcEffectsDelegate() = default;

VcEffectsDelegate::~VcEffectsDelegate() = default;

void VcEffectsDelegate::AddEffect(std::unique_ptr<VcHostedEffect> effect) {
  const auto& id = effect->id();
  if (!effects_.contains(id)) {
    effects_[id] = std::move(effect);
  }
}

void VcEffectsDelegate::RemoveEffect(VcEffectId effect_id) {
  if (features::IsVcDlcUiEnabled()) {
    // Propagate effect removal to ensure dependant `VcUiTileController`'s
    // are reset, and DLC downloads are canceled if they are in progress.
    on_effect_will_be_removed_callback_.Run(this);
  }

  effects_.erase(effect_id);
}

int VcEffectsDelegate::GetNumEffects() {
  return effects_.size();
}

const VcHostedEffect* VcEffectsDelegate::GetEffectById(VcEffectId effect_id) {
  if (effects_.contains(effect_id)) {
    return effects_[effect_id].get();
  }
  return nullptr;
}

std::vector<VcHostedEffect*> VcEffectsDelegate::GetAllEffects(
    VcEffectType type) {
  std::vector<VcHostedEffect*> effects_of_type;

  for (const auto& [effect_id, effect] : effects_) {
    if (effect->type() == type) {
      effects_of_type.push_back(effect.get());
    }
  }

  return effects_of_type;
}

std::vector<VcHostedEffect*> VcEffectsDelegate::GetAvailableEffects(
    VcEffectType type) {
  std::vector<VcHostedEffect*> effects_of_type;

  for (const auto& [effect_id, effect] : effects_) {
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

void VcEffectsDelegate::RecordInitialStates() {
  for (auto& [effect_id, effect] : effects_) {
    // Only record supported effects, because if an effect does not apply to the
    // current session the current state is not relevant.
    if (!DependenciesSatisfied(effect.get())) {
      continue;
    }

    auto current_state = effect->get_state_callback().Run();
    if (!current_state.has_value()) {
      return;
    }

    if (effect->type() == VcEffectType::kToggle) {
      CHECK_EQ(effect->GetNumStates(), 1);
      base::UmaHistogramBoolean(
          video_conference_utils::GetEffectHistogramNameForInitialState(
              effect_id),
          current_state.value());
      if (effect_id == VcEffectId::kStudioLook) {
        // Records initial preference states associated with Stduio Look effect.
        raw_ptr<CameraEffectsController> effects_controller_ =
            Shell::Get()->camera_effects_controller();
        base::UmaHistogramBoolean(
            video_conference_utils::GetEffectHistogramNameForInitialState(
                VcEffectId::kPortraitRelighting),
            *effects_controller_->GetEffectState(
                VcEffectId::kPortraitRelighting) != 0);
        base::UmaHistogramBoolean(
            video_conference_utils::GetEffectHistogramNameForInitialState(
                VcEffectId::kFaceRetouch),
            *effects_controller_->GetEffectState(VcEffectId::kFaceRetouch) !=
                0);
      }
    } else {
      RecordMetricsForSetValueEffectOnStartup(effect_id, current_state.value());
    }
  }
}

}  // namespace ash
