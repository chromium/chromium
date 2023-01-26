// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/check_is_test.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// A `std::pair` representation of the background blur state that
// `CameraHalDispatcherImpl` expects:
// - `BlurLevel` that specifies how much blur to apply
// - `bool` that's 'true' if background blur is enabled, false otherwise
using CameraHalBackgroundBlurState = std::pair<cros::mojom::BlurLevel, bool>;

// Returns 'true' if `pref_value` is an allowable value of
// `CameraEffectsController::BackgroundBlurEffectState`, 'false' otherwise.
bool IsValidBackgroundBlurState(int pref_value) {
  switch (pref_value) {
    case CameraEffectsController::BackgroundBlurEffectState::kOff:
    case CameraEffectsController::BackgroundBlurEffectState::kLowest:
    case CameraEffectsController::BackgroundBlurEffectState::kLight:
    case CameraEffectsController::BackgroundBlurEffectState::kMedium:
    case CameraEffectsController::BackgroundBlurEffectState::kHeavy:
    case CameraEffectsController::BackgroundBlurEffectState::kMaximum:
      return true;
  }

  return false;
}

// Maps `effect_state` (assumed to be a value read out of
// `prefs::kBackgroundBlur`) to a `CameraHalBackgroundBlurState` (that
// `CameraHalDispatcherImpl` expects).
CameraHalBackgroundBlurState MapBackgroundBlurEffectStateToCameraHalState(
    int effect_state) {
  DCHECK(IsValidBackgroundBlurState(effect_state));

  switch (effect_state) {
    // For state `kOff`, the `bool` is 'false' because background blur is
    // disabled, `BlurLevel` is set to `kLowest` but its value doesn't matter.
    case CameraEffectsController::BackgroundBlurEffectState::kOff:
      return std::make_pair(cros::mojom::BlurLevel::kLowest, false);

    // For states other than `kOff`, background blur is enabled so the `bool`
    // is set to 'true' and `effect_state` is mapped to a `BlurLevel`.
    case CameraEffectsController::BackgroundBlurEffectState::kLowest:
      return std::make_pair(cros::mojom::BlurLevel::kLowest, true);
    case CameraEffectsController::BackgroundBlurEffectState::kLight:
      return std::make_pair(cros::mojom::BlurLevel::kLight, true);
    case CameraEffectsController::BackgroundBlurEffectState::kMedium:
      return std::make_pair(cros::mojom::BlurLevel::kMedium, true);
    case CameraEffectsController::BackgroundBlurEffectState::kHeavy:
      return std::make_pair(cros::mojom::BlurLevel::kHeavy, true);
    case CameraEffectsController::BackgroundBlurEffectState::kMaximum:
      return std::make_pair(cros::mojom::BlurLevel::kMaximum, true);
  }

  NOTREACHED();
  return std::make_pair(cros::mojom::BlurLevel::kLowest, false);
}

// Maps the `CameraHalDispatcherImpl`-ready background blur state
// `level`/`enabled` to `CameraEffectsController::BackgroundBlurEffectState`,
// which is what's written to `prefs::kBackgroundBlur`.
CameraEffectsController::BackgroundBlurEffectState
MapBackgroundBlurCameraHalStateToEffectState(cros::mojom::BlurLevel level,
                                             bool enabled) {
  if (!enabled) {
    return CameraEffectsController::BackgroundBlurEffectState::kOff;
  }

  switch (level) {
    case cros::mojom::BlurLevel::kLowest:
      return CameraEffectsController::BackgroundBlurEffectState::kLowest;
    case cros::mojom::BlurLevel::kLight:
      return CameraEffectsController::BackgroundBlurEffectState::kLight;
    case cros::mojom::BlurLevel::kMedium:
      return CameraEffectsController::BackgroundBlurEffectState::kMedium;
    case cros::mojom::BlurLevel::kHeavy:
      return CameraEffectsController::BackgroundBlurEffectState::kHeavy;
    case cros::mojom::BlurLevel::kMaximum:
      return CameraEffectsController::BackgroundBlurEffectState::kMaximum;
  }

  NOTREACHED();
  return CameraEffectsController::BackgroundBlurEffectState::kLowest;
}

}  // namespace

CameraEffectsController::CameraEffectsController() {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  current_effects_ = cros::mojom::EffectsConfig::New();

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

CameraEffectsController::~CameraEffectsController() {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->effects_manager();
  if (effects_manager.IsDelegateRegistered(this)) {
    // The `VcEffectsDelegate` was registered, so must therefore be
    // unregistered.
    effects_manager.UnregisterDelegate(this);
  }
}

// TODO(b/265586822): this should be eventually detected from hardware support.
bool CameraEffectsController::IsCameraEffectsSupported(
    cros::mojom::CameraEffect effect) {
  switch (effect) {
    case cros::mojom::CameraEffect::kNone:
    case cros::mojom::CameraEffect::kBackgroundBlur:
    case cros::mojom::CameraEffect::kPortraitRelight:
      return features::IsVideoConferenceEnabled();
    case cros::mojom::CameraEffect::kBackgroundReplace:
      return features::IsVcBackgroundReplaceEnabled();
  }
}

cros::mojom::EffectsConfigPtr CameraEffectsController::GetCameraEffects() {
  return current_effects_.Clone();
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
  if (!IsCameraEffectsSupported()) {
    return;
  }

  // We have to register all camera effects prefs; because we need use them to
  // construct the cros::mojom::EffectsConfigPtr.
  registry->RegisterIntegerPref(prefs::kBackgroundBlur,
                                BackgroundBlurEffectState::kOff);

  registry->RegisterBooleanPref(prefs::kBackgroundReplace, false);

  registry->RegisterBooleanPref(prefs::kPortraitRelighting, false);
}

void CameraEffectsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_change_registrar_ &&
      pref_service == pref_change_registrar_->prefs()) {
    return;
  }

  // Initial login and user switching in multi profiles.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  // Initialize camera effects for the first time. Set the expected initial
  // state in case camera server isn't registered.
  SetInitialCameraEffects(GetEffectsConfigFromPref());
  // If the camera has started, it won't get the previous setting so call it
  // here too. If the camera service isn't ready it this call will be ignored.
  SetCameraEffects(GetEffectsConfigFromPref());

  // If any effects have controls the user can access, this will create the
  // effects UI and register `CameraEffectsController`'s `VcEffectsDelegate`
  // interface.
  InitializeEffectControls();
}

absl::optional<int> CameraEffectsController::GetEffectState(int effect_id) {
  switch (static_cast<cros::mojom::CameraEffect>(effect_id)) {
    case cros::mojom::CameraEffect::kBackgroundBlur:
      return MapBackgroundBlurCameraHalStateToEffectState(
          current_effects_->blur_level, current_effects_->blur_enabled);
    case cros::mojom::CameraEffect::kPortraitRelight:
      return current_effects_->relight_enabled;
    case cros::mojom::CameraEffect::kBackgroundReplace:
    case cros::mojom::CameraEffect::kNone:
      return absl::nullopt;
  }

  NOTREACHED();
  return absl::nullopt;
}

void CameraEffectsController::OnEffectControlActivated(
    absl::optional<int> effect_id,
    absl::optional<int> state) {
  DCHECK(effect_id.has_value());

  cros::mojom::EffectsConfigPtr new_effects = current_effects_.Clone();

  switch (effect_id.value()) {
    case static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur): {
      // UI should not pass in any invalid state.
      if (!state.has_value() || !IsValidBackgroundBlurState(state.value())) {
        state = static_cast<int>(
            CameraEffectsController::BackgroundBlurEffectState::kOff);
      }

      auto [blur_level, blur_enabled] =
          MapBackgroundBlurEffectStateToCameraHalState(state.value());
      new_effects->blur_level = blur_level;
      new_effects->blur_enabled = blur_enabled;
      if (new_effects->blur_enabled) {
        // background replace should be disabled since background blur is
        // enabled.
        new_effects->replace_enabled = false;
      }
      break;
    }
    case static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight): {
      new_effects->relight_enabled =
          state.value_or(!new_effects->relight_enabled);
      break;
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
    if (new_config.is_null()) {
      new_config = cros::mojom::EffectsConfig::New();
    }

    SetEffectsConfigToPref(new_config.Clone());

    current_effects_ = std::move(new_config);

    for (auto& ob : observers_) {
      ob.OnCameraEffectsChanged(current_effects_.Clone());
    }
  }
}

cros::mojom::EffectsConfigPtr
CameraEffectsController::GetEffectsConfigFromPref() {
  cros::mojom::EffectsConfigPtr effects = cros::mojom::EffectsConfig::New();
  if (!pref_change_registrar_ || !pref_change_registrar_->prefs()) {
    return effects;
  }

  int background_blur_state_in_pref =
      pref_change_registrar_->prefs()->GetInteger(prefs::kBackgroundBlur);
  if (!IsValidBackgroundBlurState(background_blur_state_in_pref)) {
    LOG(ERROR) << __FUNCTION__ << " background_blur_state_in_pref "
               << background_blur_state_in_pref
               << " is NOT a valid background blur effect state, using kOff";
    background_blur_state_in_pref =
        static_cast<int>(BackgroundBlurEffectState::kOff);
  }

  CameraHalBackgroundBlurState blur_state =
      MapBackgroundBlurEffectStateToCameraHalState(
          background_blur_state_in_pref);
  effects->blur_enabled = blur_state.second;
  effects->blur_level = blur_state.first;

  effects->replace_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kBackgroundReplace);
  effects->relight_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kPortraitRelighting);
  return effects;
}

void CameraEffectsController::SetEffectsConfigToPref(
    cros::mojom::EffectsConfigPtr new_config) {
  if (!pref_change_registrar_ || !pref_change_registrar_->prefs()) {
    return;
  }

  if (new_config->blur_enabled != current_effects_->blur_enabled ||
      new_config->blur_level != current_effects_->blur_level) {
    pref_change_registrar_->prefs()->SetInteger(
        prefs::kBackgroundBlur,
        MapBackgroundBlurCameraHalStateToEffectState(new_config->blur_level,
                                                     new_config->blur_enabled));
  }

  if (new_config->replace_enabled != current_effects_->replace_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(prefs::kBackgroundReplace,
                                                new_config->replace_enabled);
  }

  if (new_config->relight_enabled != current_effects_->relight_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(prefs::kPortraitRelighting,
                                                new_config->relight_enabled);
  }
}

bool CameraEffectsController::IsEffectControlAvailable(
    cros::mojom::CameraEffect effect /* = cros::mojom::CameraEffect::kNone*/) {
  if (!ash::features::IsVideoConferenceEnabled()) {
    return false;
  }

  switch (effect) {
    case cros::mojom::CameraEffect::kNone:
      // Return 'true' if any effect is available.
      return IsCameraEffectsSupported(
                 cros::mojom::CameraEffect::kBackgroundBlur) ||
             IsCameraEffectsSupported(
                 cros::mojom::CameraEffect::kPortraitRelight);
    case cros::mojom::CameraEffect::kBackgroundBlur:
      return IsCameraEffectsSupported(
          cros::mojom::CameraEffect::kBackgroundBlur);
    case cros::mojom::CameraEffect::kPortraitRelight:
      return IsCameraEffectsSupported(
          cros::mojom::CameraEffect::kPortraitRelight);
    case cros::mojom::CameraEffect::kBackgroundReplace:
      return false;
  }

  return false;
}

void CameraEffectsController::InitializeEffectControls() {
  if (VideoConferenceTrayController::Get()
          ->effects_manager()
          .IsDelegateRegistered(this)) {
    return;
  }

  // If background blur UI controls are present, construct the effect and its
  // states.
  if (IsEffectControlAvailable(cros::mojom::CameraEffect::kBackgroundBlur)) {
    auto effect = std::make_unique<VcHostedEffect>(
        VcEffectType::kSetValue,
        base::BindRepeating(
            &CameraEffectsController::GetEffectState, base::Unretained(this),
            static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)));
    effect->set_label_text(l10n_util::GetStringUTF16(
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_NAME));
    effect->set_id(
        static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur));
    AddBackgroundBlurStateToEffect(
        effect.get(),
        /*state_value=*/BackgroundBlurEffectState::kOff,
        /*string_id=*/IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_OFF);
    AddBackgroundBlurStateToEffect(
        effect.get(),
        /*state_value=*/BackgroundBlurEffectState::kLight,
        /*string_id=*/IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_LIGHT);
    AddBackgroundBlurStateToEffect(
        effect.get(),
        /*state_value=*/BackgroundBlurEffectState::kMaximum,
        /*string_id=*/
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_FULL);
    AddEffect(std::move(effect));
  }

  // If portrait relight UI controls are present, construct the effect
  // and its state.
  if (IsEffectControlAvailable(cros::mojom::CameraEffect::kPortraitRelight)) {
    std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
        VcEffectType::kToggle,
        base::BindRepeating(
            &CameraEffectsController::GetEffectState, base::Unretained(this),
            static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight)));
    effect->set_id(
        static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight));
    effect->AddState(std::make_unique<VcEffectState>(
        /*icon=*/&kPrivacyIndicatorsCameraIcon,
        /*label_text=*/
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_BUBBLE_PORTRAIT_RELIGHT_NAME),
        /*accessible_name_id=*/
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_PORTRAIT_RELIGHT_NAME,
        /*button_callback=*/
        base::BindRepeating(
            &CameraEffectsController::OnEffectControlActivated,
            base::Unretained(this),
            /*effect_id=*/
            static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight),
            /*value=*/absl::nullopt)));
    AddEffect(std::move(effect));
  }

  // If *any* effects' UI controls are present, register with the effects
  // manager.
  if (IsEffectControlAvailable()) {
    VideoConferenceTrayController::Get()->effects_manager().RegisterDelegate(
        this);
  }
}

void CameraEffectsController::AddBackgroundBlurStateToEffect(
    VcHostedEffect* effect,
    int state_value,
    int string_id) {
  DCHECK(effect);
  // TODO(b/265200087): Replace the icon with the proper icon per effect.
  effect->AddState(std::make_unique<VcEffectState>(
      /*icon=*/&ash::kPrivacyIndicatorsCameraIcon,
      /*label_text=*/l10n_util::GetStringUTF16(string_id),
      /*accessible_name_id=*/string_id,
      /*button_callback=*/
      base::BindRepeating(
          &CameraEffectsController::OnEffectControlActivated,
          weak_factory_.GetWeakPtr(),
          /*effect_id=*/
          static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
          /*value=*/state_value),
      /*state=*/state_value));
}

}  // namespace ash
