// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check_is_test.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// The value stored in pref to indicate that background blur is disabled.
constexpr int kBackgroundBlurLevelForDisabling = -1;

// Gets blur level from chrome flag.
cros::mojom::BlurLevel GetBlurLevelFromFlag() {
  std::string blur_level = GetFieldTrialParamValueByFeature(
      ash::features::kVCBackgroundBlur, "blur_level");
  if (blur_level == "lowest") {
    return cros::mojom::BlurLevel::kLowest;
  } else if (blur_level == "light") {
    return cros::mojom::BlurLevel::kLight;
  } else if (blur_level == "medium") {
    return cros::mojom::BlurLevel::kMedium;
  } else if (blur_level == "heavy") {
    return cros::mojom::BlurLevel::kHeavy;
  } else if (blur_level == "maximum") {
    return cros::mojom::BlurLevel::kMaximum;
  }
  return cros::mojom::BlurLevel::kLowest;
}

// The default enable state for all camera effects should be false; but here we
// use the IsCameraEffectsSupported instead until we are able to enable these
// prefs from the ui.
cros::mojom::EffectsConfigPtr GetInitialCameraEffects() {
  cros::mojom::EffectsConfigPtr config = cros::mojom::EffectsConfig::New();
  config->blur_enabled = CameraEffectsController::IsCameraEffectsSupported(
      cros::mojom::CameraEffect::kBackgroundBlur);
  config->blur_level = GetBlurLevelFromFlag();
  // We don't want enable a conflict config, so kBackgroundReplace is set to
  // enabled only if kBackgroundBlur is not.
  config->replace_enabled =
      CameraEffectsController::IsCameraEffectsSupported(
          cros::mojom::CameraEffect::kBackgroundReplace) &&
      !CameraEffectsController::IsCameraEffectsSupported(
          cros::mojom::CameraEffect::kBackgroundBlur);
  config->relight_enabled = CameraEffectsController::IsCameraEffectsSupported(
      cros::mojom::CameraEffect::kPortraitRelight);

  return config;
}

}  // namespace

CameraEffectsController::CameraEffectsController() {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  current_effects_ = GetInitialCameraEffects();

  media::CameraHalDispatcherImpl::GetInstance()
      ->SetCameraEffectsControllerCallback(
          // The callback passed to CameraHalDispatcherImpl will be called on a
          // different thread inside CameraHalDispatcherImpl, so we need always
          // post the callback onto current task runner.
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(
                  &CameraEffectsController::OnNewCameraEffectsSet,
                  weak_factory_.GetWeakPtr())));
}

CameraEffectsController::~CameraEffectsController() = default;

bool CameraEffectsController::IsCameraEffectsSupported(
    cros::mojom::CameraEffect effect) {
  switch (effect) {
    case cros::mojom::CameraEffect::kBackgroundBlur:
      return features::IsVCBackgroundBlurEnabled();
    case cros::mojom::CameraEffect::kBackgroundReplace:
      return features::IsVCBackgroundReplaceEnabled();
    case cros::mojom::CameraEffect::kPortraitRelight:
      return features::IsVCPortraitRelightingEnabled();

    // returns if any effects is supported for kNone.
    case cros::mojom::CameraEffect::kNone:
      return features::IsVCBackgroundBlurEnabled() ||
             features::IsVCBackgroundReplaceEnabled() ||
             features::IsVCPortraitRelightingEnabled();
  }
}

cros::mojom::EffectsConfigPtr CameraEffectsController::GetCameraEffects() {
  DCHECK(pref_change_registrar_->prefs());
  return GetEffectsConfigFromPref();
}

void CameraEffectsController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}
void CameraEffectsController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void CameraEffectsController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  if (!IsCameraEffectsSupported())
    return;

  // We have to register all camera effects prefs; because we need use them to
  // construct the cros::mojom::EffectsConfigPtr.
  registry->RegisterIntegerPref(prefs::kBackgroundBlur,
                                GetInitialCameraEffects()->blur_enabled
                                    ? static_cast<int>(GetBlurLevelFromFlag())
                                    : kBackgroundBlurLevelForDisabling);

  registry->RegisterBooleanPref(prefs::kBackgroundReplace,
                                GetInitialCameraEffects()->replace_enabled);

  registry->RegisterBooleanPref(prefs::kPortraitRelighting,
                                GetInitialCameraEffects()->relight_enabled);
}

void CameraEffectsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_change_registrar_ && pref_service == pref_change_registrar_->prefs())
    return;

  // Initial login and user switching in multi profiles.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  auto callback =
      effect_result_for_testing_.has_value()
          ? base::BindRepeating(
                &CameraEffectsController::OnCameraEffectsPrefChanged,
                weak_factory_.GetWeakPtr())
          : base::BindPostTask(
                base::SequencedTaskRunner::GetCurrentDefault(),
                base::BindRepeating(
                    &CameraEffectsController::OnCameraEffectsPrefChanged,
                    weak_factory_.GetWeakPtr()));

  pref_change_registrar_->Add(prefs::kBackgroundBlur, callback);

  pref_change_registrar_->Add(prefs::kBackgroundReplace, callback);

  pref_change_registrar_->Add(prefs::kPortraitRelighting, callback);

  // Initialize camera effects for the first time. Set the expected initial
  // state in case camera server isn't registered.
  SetInitialCameraEffects(GetEffectsConfigFromPref());
  // If the camera has started, it won't get the previous setting so call it
  // here too. If the camera service isn't ready it this call will be ignored.
  SetCameraEffects(GetEffectsConfigFromPref());
}

void CameraEffectsController::OnCameraEffectsPrefChanged(
    const std::string& pref_name) {
  cros::mojom::EffectsConfigPtr new_effects = GetEffectsConfigFromPref();

  if (pref_name == prefs::kBackgroundBlur) {
    // Skip if current applied effect is already the same as new value.Supported
    if (current_effects_->blur_enabled == new_effects->blur_enabled &&
        current_effects_->blur_level == new_effects->blur_level) {
      return;
    }

    if (new_effects->blur_enabled) {
      // background replace should be disabled since background blur is enabled.
      new_effects->replace_enabled = false;
    }

  } else if (pref_name == prefs::kBackgroundReplace) {
    // Skip if current applied effect is already the same as new value.
    if (current_effects_->replace_enabled == new_effects->replace_enabled) {
      return;
    }

    if (new_effects->replace_enabled) {
      // background blur should be disabled since background replace is enabled.
      new_effects->blur_enabled = false;
    }
  } else if (pref_name == prefs::kPortraitRelighting) {
    // Skip if current applied effect is already the same as new value.
    if (current_effects_->relight_enabled == new_effects->relight_enabled) {
      return;
    }
  }

  SetCameraEffects(std::move(new_effects));
}

void CameraEffectsController::SetCameraEffects(
    cros::mojom::EffectsConfigPtr config) {
  // For backwards compatibility, will be removed after mojom is updated.
  if (config->blur_enabled) {
    config->effect = cros::mojom::CameraEffect::kBackgroundBlur;
  }
  if (config->replace_enabled) {
    config->effect = cros::mojom::CameraEffect::kBackgroundReplace;
  }
  if (config->relight_enabled) {
    config->effect = cros::mojom::CameraEffect::kPortraitRelight;
  }

  // Directly calls the callback for testing case.
  if (effect_result_for_testing_.has_value()) {
    CHECK_IS_TEST();
    OnNewCameraEffectsSet(std::move(config),
                          effect_result_for_testing_.value());
  } else {
    media::CameraHalDispatcherImpl::GetInstance()->SetCameraEffects(
        std::move(config));
  }
}

void CameraEffectsController::SetInitialCameraEffects(
    cros::mojom::EffectsConfigPtr config) {
  media::CameraHalDispatcherImpl::GetInstance()->SetInitialCameraEffects(
      std::move(config));
}

void CameraEffectsController::OnNewCameraEffectsSet(
    cros::mojom::EffectsConfigPtr new_config,
    cros::mojom::SetEffectResult result) {
  // If SetCamerEffects succeeded, update `current_effects_` and notify all
  // observers.
  // This callback with null EffectsConfigPtr indicates that
  // (1) The last SetCamerEffect failed.
  // (2) It was the first SetCamerEffect call after the camera stack
  // initialized; so no camera effects were applied before that. Assuming this
  // does not happen very often, the only way to keep the pref to be consisitent
  // with the prefs is to reset everything.
  if (result == cros::mojom::SetEffectResult::kOk || new_config.is_null()) {
    current_effects_ = new_config.is_null() ? GetInitialCameraEffects()
                                            : std::move(new_config);

    for (auto& ob : observers_) {
      ob.OnCameraEffectsChanged(current_effects_.Clone());
    }
  }

  // Always update prefs with `current_effects_`.
  // (1) For "result == kOk", the prefs are updated with the new effects.
  // (2) Otherwise, the prefs changes are effectively reverted.
  SetEffectsConfigToPref(current_effects_.Clone());
}

cros::mojom::EffectsConfigPtr
CameraEffectsController::GetEffectsConfigFromPref() {
  cros::mojom::EffectsConfigPtr effects = cros::mojom::EffectsConfig::New();
  const int background_level_in_pref =
      pref_change_registrar_->prefs()->GetInteger(prefs::kBackgroundBlur);

  effects->blur_enabled =
      background_level_in_pref != kBackgroundBlurLevelForDisabling;

  effects->blur_level =
      effects->blur_enabled
          ? static_cast<cros::mojom::BlurLevel>(background_level_in_pref)
          : GetBlurLevelFromFlag();

  effects->replace_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kBackgroundReplace);
  effects->relight_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kPortraitRelighting);
  return effects;
}

void CameraEffectsController::SetEffectsConfigToPref(
    cros::mojom::EffectsConfigPtr new_config) {
  cros::mojom::EffectsConfigPtr old_effects = GetEffectsConfigFromPref();

  if (new_config->blur_enabled != old_effects->blur_enabled ||
      new_config->blur_level != old_effects->blur_level) {
    pref_change_registrar_->prefs()->SetInteger(
        prefs::kBackgroundBlur, new_config->blur_enabled
                                    ? static_cast<int>(new_config->blur_level)
                                    : kBackgroundBlurLevelForDisabling);
  }

  if (new_config->replace_enabled != old_effects->replace_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(prefs::kBackgroundReplace,
                                                new_config->replace_enabled);
  }

  if (new_config->relight_enabled != old_effects->relight_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(prefs::kPortraitRelighting,
                                                new_config->relight_enabled);
  }
}

}  // namespace ash
