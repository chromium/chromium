// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

// A `std::pair` representation of the background blur state that
// `CameraHalDispatcherImpl` expects:
// - `BlurLevel` that specifies how much blur to apply
// - `bool` that's 'true' if background blur is enabled, false otherwise
using CameraHalBackgroundBlurState = std::pair<cros::mojom::BlurLevel, bool>;

// Returns 'true' if `pref_value` is an allowable value of
// `CameraEffectsController::BackgroundBlurPrefValue`, 'false' otherwise.
bool IsValidBackgroundBlurPrefValue(int pref_value) {
  switch (pref_value) {
    case CameraEffectsController::BackgroundBlurPrefValue::kOff:
    case CameraEffectsController::BackgroundBlurPrefValue::kLowest:
    case CameraEffectsController::BackgroundBlurPrefValue::kLight:
    case CameraEffectsController::BackgroundBlurPrefValue::kMedium:
    case CameraEffectsController::BackgroundBlurPrefValue::kHeavy:
    case CameraEffectsController::BackgroundBlurPrefValue::kMaximum:
      return true;
  }

  return false;
}

// Maps `pref_value` (assumed to be a value read out of
// `prefs::kBackgroundBlur`) to a `CameraHalBackgroundBlurState` (that
// `CameraHalDispatcherImpl` expects).
CameraHalBackgroundBlurState MapBackgroundBlurPrefValueToCameraHalState(
    int pref_value) {
  DCHECK(IsValidBackgroundBlurPrefValue(pref_value));

  switch (pref_value) {
    // For state `kOff`, the `bool` is 'false' because background blur is
    // disabled, `BlurLevel` is set to `kLowest` but its value doesn't matter.
    case CameraEffectsController::BackgroundBlurPrefValue::kOff:
      return std::make_pair(cros::mojom::BlurLevel::kLowest, false);

    // For states other than `kOff`, background blur is enabled so the `bool`
    // is set to 'true' and `pref_value` is mapped to a `BlurLevel`.
    case CameraEffectsController::BackgroundBlurPrefValue::kLowest:
      return std::make_pair(cros::mojom::BlurLevel::kLowest, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kLight:
      return std::make_pair(cros::mojom::BlurLevel::kLight, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kMedium:
      return std::make_pair(cros::mojom::BlurLevel::kMedium, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kHeavy:
      return std::make_pair(cros::mojom::BlurLevel::kHeavy, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kMaximum:
      return std::make_pair(cros::mojom::BlurLevel::kMaximum, true);
  }

  NOTREACHED();
  return std::make_pair(cros::mojom::BlurLevel::kLowest, false);
}

// Maps the `CameraHalDispatcherImpl`-ready background blur state
// `level`/`enabled` to `CameraEffectsController::BackgroundBlurPrefValue`,
// which is what's written to `prefs::kBackgroundBlur`.
CameraEffectsController::BackgroundBlurPrefValue
MapBackgroundBlurCameraHalStateToPrefValue(cros::mojom::BlurLevel level,
                                           bool enabled) {
  if (!enabled) {
    return CameraEffectsController::BackgroundBlurPrefValue::kOff;
  }

  switch (level) {
    case cros::mojom::BlurLevel::kLowest:
      return CameraEffectsController::BackgroundBlurPrefValue::kLowest;
    case cros::mojom::BlurLevel::kLight:
      return CameraEffectsController::BackgroundBlurPrefValue::kLight;
    case cros::mojom::BlurLevel::kMedium:
      return CameraEffectsController::BackgroundBlurPrefValue::kMedium;
    case cros::mojom::BlurLevel::kHeavy:
      return CameraEffectsController::BackgroundBlurPrefValue::kHeavy;
    case cros::mojom::BlurLevel::kMaximum:
      return CameraEffectsController::BackgroundBlurPrefValue::kMaximum;
  }

  NOTREACHED();
  return CameraEffectsController::BackgroundBlurPrefValue::kLowest;
}

CameraEffectsController::BackgroundBlurState MapBackgroundBlurPrefValueToState(
    int pref_value) {
  DCHECK(IsValidBackgroundBlurPrefValue(pref_value));

  switch (pref_value) {
    case CameraEffectsController::BackgroundBlurPrefValue::kOff:
      return CameraEffectsController::BackgroundBlurState::kOff;
    case CameraEffectsController::BackgroundBlurPrefValue::kLowest:
      return CameraEffectsController::BackgroundBlurState::kLowest;
    case CameraEffectsController::BackgroundBlurPrefValue::kLight:
      return CameraEffectsController::BackgroundBlurState::kLight;
    case CameraEffectsController::BackgroundBlurPrefValue::kMedium:
      return CameraEffectsController::BackgroundBlurState::kMedium;
    case CameraEffectsController::BackgroundBlurPrefValue::kHeavy:
      return CameraEffectsController::BackgroundBlurState::kHeavy;
    case CameraEffectsController::BackgroundBlurPrefValue::kMaximum:
      return CameraEffectsController::BackgroundBlurState::kMaximum;
  }

  NOTREACHED();
  return CameraEffectsController::BackgroundBlurState::kOff;
}

}  // namespace

CameraEffectsController::CameraEffectsController()
    : main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  current_effects_ = cros::mojom::EffectsConfig::New();

  // The effects are not applied when this is constructed, observe for changes
  // that will come later.
  media::CameraHalDispatcherImpl::GetInstance()->AddCameraEffectObserver(
      this, base::DoNothing());

  Shell::Get()->autozoom_controller()->AddObserver(this);
}

CameraEffectsController::~CameraEffectsController() {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->effects_manager();
  if (effects_manager.IsDelegateRegistered(this)) {
    // The `VcEffectsDelegate` was registered, so must therefore be
    // unregistered.
    effects_manager.UnregisterDelegate(this);
  }

  Shell::Get()->autozoom_controller()->RemoveObserver(this);
  media::CameraHalDispatcherImpl::GetInstance()->RemoveCameraEffectObserver(
      this);
}

cros::mojom::EffectsConfigPtr CameraEffectsController::GetCameraEffects() {
  return current_effects_.Clone();
}

// static
void CameraEffectsController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  if (!features::IsVideoConferenceEnabled()) {
    return;
  }

  // We have to register all camera effects prefs; because we need use them to
  // construct the cros::mojom::EffectsConfigPtr.
  registry->RegisterIntegerPref(prefs::kBackgroundBlur,
                                BackgroundBlurPrefValue::kOff);

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

  // If the camera has started, it won't get the previous setting so call it
  // here too. If the camera service isn't ready it this call will be ignored.
  SetCameraEffects(GetEffectsConfigFromPref());

  // If any effects have controls the user can access, this will create the
  // effects UI and register `CameraEffectsController`'s `VcEffectsDelegate`
  // interface.
  InitializeEffectControls();
}

absl::optional<int> CameraEffectsController::GetEffectState(
    VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kBackgroundBlur:
      return MapBackgroundBlurCameraHalStateToPrefValue(
          current_effects_->blur_level, current_effects_->blur_enabled);
    case VcEffectId::kPortraitRelighting:
      return current_effects_->relight_enabled;
    case VcEffectId::kCameraFraming:
      return Shell::Get()->autozoom_controller()->GetState() !=
             cros::mojom::CameraAutoFramingState::OFF;
    case VcEffectId::kNoiseCancellation:
    case VcEffectId::kLiveCaption:
    case VcEffectId::kTestEffect:
      NOTREACHED();
      return absl::nullopt;
  }
}

void CameraEffectsController::OnEffectControlActivated(
    VcEffectId effect_id,
    absl::optional<int> state) {
  cros::mojom::EffectsConfigPtr new_effects = current_effects_.Clone();

  switch (effect_id) {
    case VcEffectId::kBackgroundBlur: {
      // UI should not pass in any invalid state.
      if (!state.has_value() ||
          !IsValidBackgroundBlurPrefValue(state.value())) {
        state = static_cast<int>(
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
      }

      auto [blur_level, blur_enabled] =
          MapBackgroundBlurPrefValueToCameraHalState(state.value());
      new_effects->blur_level = blur_level;
      new_effects->blur_enabled = blur_enabled;
      if (new_effects->blur_enabled) {
        // background replace should be disabled since background blur is
        // enabled.
        new_effects->replace_enabled = false;
      }
      break;
    }
    case VcEffectId::kPortraitRelighting: {
      new_effects->relight_enabled =
          state.value_or(!new_effects->relight_enabled);
      break;
    }
    case VcEffectId::kCameraFraming: {
      Shell::Get()->autozoom_controller()->Toggle();
      break;
    }
    case VcEffectId::kNoiseCancellation:
    case VcEffectId::kLiveCaption:
    case VcEffectId::kTestEffect:
      NOTREACHED();
      return;
  }

  SetCameraEffects(std::move(new_effects));
}

void CameraEffectsController::RecordMetricsForSetValueEffectOnClick(
    VcEffectId effect_id,
    int state_value) const {
  // `CameraEffectsController` currently only has background blur as a set-value
  // effect, so it shouldn't be any other effects here.
  DCHECK_EQ(VcEffectId::kBackgroundBlur, effect_id);

  base::UmaHistogramEnumeration(
      video_conference_utils::GetEffectHistogramNameForClick(effect_id),
      MapBackgroundBlurPrefValueToState(state_value));
}

void CameraEffectsController::RecordMetricsForSetValueEffectOnStartup(
    VcEffectId effect_id,
    int state_value) const {
  // `CameraEffectsController` currently only has background blur as a set-value
  // effect, so it shouldn't be any other effects here.
  DCHECK_EQ(VcEffectId::kBackgroundBlur, effect_id);

  base::UmaHistogramEnumeration(
      video_conference_utils::GetEffectHistogramNameForInitialState(effect_id),
      MapBackgroundBlurPrefValueToState(state_value));
}

void CameraEffectsController::OnCameraEffectChanged(
    const cros::mojom::EffectsConfigPtr& new_effects) {
  // As `CameraHalDispatcher` notifies the `new_effects` from a different
  // thread, we want to ensure the `current_effects_` is always accessed through
  // the `main_task_runner_`.
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraEffectsController::OnCameraEffectChanged,
                       weak_factory_.GetWeakPtr(), new_effects.Clone()));
    return;
  }

  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // If `SetCamerEffects()` finished, update `current_effects_` and prefs.
  if (!new_effects.is_null()) {
    SetEffectsConfigToPref(new_effects.Clone());
    current_effects_ = new_effects.Clone();
  }
}

void CameraEffectsController::OnAutozoomControlEnabledChanged(bool enabled) {
  if (!enabled) {
    RemoveEffect(VcEffectId::kCameraFraming);
    return;
  }

  // Using `base::Unretained()` is safe here since `this` owns the created
  // VcHostedEffect after calling `AddEffect()` below.
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&CameraEffectsController::GetEffectState,
                          base::Unretained(this), VcEffectId::kCameraFraming),
      /*effect_id=*/VcEffectId::kCameraFraming);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kVideoConferenceCameraFramingOnIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL,
      /*button_callback=*/
      base::BindRepeating(&CameraEffectsController::OnEffectControlActivated,
                          base::Unretained(this),
                          /*effect_id=*/VcEffectId::kCameraFraming,
                          /*value=*/absl::nullopt));
  effect_state->set_disabled_icon(&kVideoConferenceCameraFramingOffIcon);
  effect->AddState(std::move(effect_state));

  effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kCamera);
  AddEffect(std::move(effect));
}

cros::mojom::SegmentationModel
CameraEffectsController::GetSegmentationModelType() {
  cros::mojom::SegmentationModel model_type =
      cros::mojom::SegmentationModel::kHighResolution;
  const std::string segmentation_model_param = GetFieldTrialParamValueByFeature(
      ash::features::kVcSegmentationModel, "segmentation_model");

  if (segmentation_model_param == "lower_resolution") {
    model_type = cros::mojom::SegmentationModel::kLowerResolution;
  }

  return model_type;
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

  // Update effects config with settings from feature flags.
  config->segmentation_model = GetSegmentationModelType();
  config->light_intensity = GetFieldTrialParamByFeatureAsDouble(
      ash::features::kVcLightIntensity, "light_intensity", 1.0);

  // Directly calls the callback for testing case.
  if (in_testing_mode_) {
    CHECK_IS_TEST();
    OnCameraEffectChanged(std::move(config));
  } else {
    media::CameraHalDispatcherImpl::GetInstance()->SetCameraEffects(
        std::move(config));
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
  if (!IsValidBackgroundBlurPrefValue(background_blur_state_in_pref)) {
    LOG(ERROR) << __FUNCTION__ << " background_blur_state_in_pref "
               << background_blur_state_in_pref
               << " is NOT a valid background blur effect state, using kOff";
    background_blur_state_in_pref = BackgroundBlurPrefValue::kOff;
  }

  CameraHalBackgroundBlurState blur_state =
      MapBackgroundBlurPrefValueToCameraHalState(background_blur_state_in_pref);
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
        MapBackgroundBlurCameraHalStateToPrefValue(new_config->blur_level,
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
  switch (effect) {
    case cros::mojom::CameraEffect::kNone:
    case cros::mojom::CameraEffect::kBackgroundBlur:
      return features::IsVideoConferenceEnabled();
    case cros::mojom::CameraEffect::kPortraitRelight:
      return features::IsVcPortraitRelightEnabled();
    case cros::mojom::CameraEffect::kBackgroundReplace:
      return features::IsVcBackgroundReplaceEnabled();
  }
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
        /*type=*/VcEffectType::kSetValue,
        /*get_state_callback=*/
        base::BindRepeating(&CameraEffectsController::GetEffectState,
                            base::Unretained(this),
                            VcEffectId::kBackgroundBlur),
        /*effect_id=*/VcEffectId::kBackgroundBlur);
    effect->set_label_text(l10n_util::GetStringUTF16(
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_NAME));
    effect->set_effects_delegate(this);
    AddBackgroundBlurStateToEffect(
        effect.get(), kVideoConferenceBackgroundBlurOffIcon,
        /*state_value=*/BackgroundBlurPrefValue::kOff,
        /*string_id=*/IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_OFF);
    AddBackgroundBlurStateToEffect(
        effect.get(), kVideoConferenceBackgroundBlurLightIcon,
        /*state_value=*/BackgroundBlurPrefValue::kLight,
        /*string_id=*/IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_LIGHT);
    AddBackgroundBlurStateToEffect(
        effect.get(), kVideoConferenceBackgroundBlurMaximumIcon,
        /*state_value=*/BackgroundBlurPrefValue::kMaximum,
        /*string_id=*/
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_FULL);
    effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kCamera);
    AddEffect(std::move(effect));
  }

  // If portrait relight UI controls are present, construct the effect
  // and its state.
  if (IsEffectControlAvailable(cros::mojom::CameraEffect::kPortraitRelight)) {
    std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
        /*type=*/VcEffectType::kToggle,
        /*get_state_callback=*/
        base::BindRepeating(&CameraEffectsController::GetEffectState,
                            base::Unretained(this),
                            VcEffectId::kPortraitRelighting),
        /*effect_id=*/VcEffectId::kPortraitRelighting);

    auto effect_state = std::make_unique<VcEffectState>(
        /*icon=*/&kVideoConferencePortraitRelightOnIcon,
        /*label_text=*/
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_BUBBLE_PORTRAIT_RELIGHT_NAME),
        /*accessible_name_id=*/
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_PORTRAIT_RELIGHT_NAME,
        /*button_callback=*/
        base::BindRepeating(&CameraEffectsController::OnEffectControlActivated,
                            base::Unretained(this),
                            /*effect_id=*/VcEffectId::kPortraitRelighting,
                            /*value=*/absl::nullopt));
    effect_state->set_disabled_icon(&kVideoConferencePortraitRelightOffIcon);
    effect->AddState(std::move(effect_state));

    effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kCamera);
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
    const gfx::VectorIcon& icon,
    int state_value,
    int string_id) {
  DCHECK(effect);
  effect->AddState(std::make_unique<VcEffectState>(
      &icon,
      /*label_text=*/l10n_util::GetStringUTF16(string_id),
      /*accessible_name_id=*/string_id,
      /*button_callback=*/
      base::BindRepeating(&CameraEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kBackgroundBlur,
                          /*value=*/state_value),
      /*state=*/state_value));
}

}  // namespace ash
