// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_effects_controller.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/notreached.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/live_caption/caption_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

bool IsStyleTransferSupportedByVc() {
  return CrasAudioHandler::Get()->IsStyleTransferSupportedForDevice(
      CrasAudioHandler::Get()->GetPrimaryActiveInputNode());
}

// Vc can only support either noise cancellation or style transfer. So we skip
// noise cancellation if style transfer is supported already.
bool IsNoiseCancellationSupportedByVc() {
  return CrasAudioHandler::Get()->IsNoiseCancellationSupportedForDevice(
             CrasAudioHandler::Get()->GetPrimaryActiveInputNode()) &&
         !IsStyleTransferSupportedByVc();
}

AudioEffectsController::AudioEffectsController() {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

AudioEffectsController::~AudioEffectsController() {
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();
  if (effects_manager.IsDelegateRegistered(this)) {
    effects_manager.UnregisterDelegate(this);
  }
}

bool AudioEffectsController::IsEffectSupported(VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kNoiseCancellation:
      return IsNoiseCancellationSupportedByVc();
    case VcEffectId::kStyleTransfer:
      return IsStyleTransferSupportedByVc();
    case VcEffectId::kLiveCaption:
      return base::FeatureList::IsEnabled(
                 features::kShowLiveCaptionInVideoConferenceTray) &&
             captions::IsLiveCaptionFeatureSupported();
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kCameraFraming:
    case VcEffectId::kTestEffect:
    case VcEffectId::kFaceRetouch:
    case VcEffectId::kStudioLook:
      NOTREACHED();
  }
}

std::optional<int> AudioEffectsController::GetEffectState(
    VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kNoiseCancellation:
      return CrasAudioHandler::Get()->GetNoiseCancellationState() ? 1 : 0;
    case VcEffectId::kStyleTransfer:
      return CrasAudioHandler::Get()->GetStyleTransferState() ? 1 : 0;
    case VcEffectId::kLiveCaption:
      return Shell::Get()->accessibility_controller()->live_caption().enabled()
                 ? 1
                 : 0;
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kCameraFraming:
    case VcEffectId::kTestEffect:
    case VcEffectId::kFaceRetouch:
    case VcEffectId::kStudioLook:
      NOTREACHED();
  }
}

void AudioEffectsController::OnEffectControlActivated(
    VcEffectId effect_id,
    std::optional<int> value) {
  switch (effect_id) {
    case VcEffectId::kNoiseCancellation: {
      // Toggle noise cancellation.
      CrasAudioHandler::Get()->SetNoiseCancellationState(
          !CrasAudioHandler::Get()->GetNoiseCancellationState(),
          CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);
      return;
    }

    case VcEffectId::kStyleTransfer: {
      // Toggle studio mic.
      CrasAudioHandler::Get()->SetStyleTransferState(
          !CrasAudioHandler::Get()->GetStyleTransferState());
      return;
    }

    case VcEffectId::kLiveCaption: {
      // Toggle live caption.
      AccessibilityController* controller =
          Shell::Get()->accessibility_controller();
      controller->live_caption().SetEnabled(
          !controller->live_caption().enabled());
      return;
    }
    case VcEffectId::kBackgroundBlur:
    case VcEffectId::kPortraitRelighting:
    case VcEffectId::kCameraFraming:
    case VcEffectId::kTestEffect:
    case VcEffectId::kFaceRetouch:
    case VcEffectId::kStudioLook:
      NOTREACHED();
  }
}

void AudioEffectsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (IsEffectSupported(VcEffectId::kNoiseCancellation)) {
    AddNoiseCancellationEffect();
  }

  if (IsEffectSupported(VcEffectId::kStyleTransfer)) {
    AddStyleTransferEffect();
  }

  if (IsEffectSupported(VcEffectId::kLiveCaption)) {
    AddLiveCaptionEffect();
  }
}

void AudioEffectsController::OnActiveInputNodeChanged() {
  RefreshNoiseCancellationOrStyleTransferSupported();
}

void AudioEffectsController::OnAudioNodesChanged() {
  RefreshNoiseCancellationOrStyleTransferSupported();
}

void AudioEffectsController::OnActiveOutputNodeChanged() {
  RefreshNoiseCancellationOrStyleTransferSupported();
}

void AudioEffectsController::OnNoiseCancellationStateChanged() {
  RefreshNoiseCancellationOrStyleTransferSupported();
}

void AudioEffectsController::OnStyleTransferStateChanged() {
  RefreshNoiseCancellationOrStyleTransferSupported();
}

void AudioEffectsController::
    RefreshNoiseCancellationOrStyleTransferSupported() {
  const bool noise_cancellation_supported =
      IsEffectSupported(VcEffectId::kNoiseCancellation);
  const bool style_transfer_supported =
      IsEffectSupported(VcEffectId::kStyleTransfer);

  const bool noise_cancellation_added =
      IsEffectsAdded(VcEffectId::kNoiseCancellation);
  const bool style_transfer_added = IsEffectsAdded(VcEffectId::kStyleTransfer);

  // If effects added match effects supported, then no action required.
  if (noise_cancellation_supported == noise_cancellation_added &&
      style_transfer_supported == style_transfer_added) {
    return;
  }

  if (style_transfer_supported != style_transfer_added) {
    if (style_transfer_supported) {
      AddStyleTransferEffect();
    } else {
      RemoveEffect(VcEffectId::kStyleTransfer);
    }

    VideoConferenceTrayController::Get()
        ->GetEffectsManager()
        .NotifyEffectSupportStateChanged(VcEffectId::kStyleTransfer,
                                         style_transfer_supported);
  }

  if (noise_cancellation_supported != noise_cancellation_added) {
    if (noise_cancellation_supported) {
      AddNoiseCancellationEffect();
    } else {
      RemoveEffect(VcEffectId::kNoiseCancellation);
    }

    VideoConferenceTrayController::Get()
        ->GetEffectsManager()
        .NotifyEffectSupportStateChanged(VcEffectId::kNoiseCancellation,
                                         noise_cancellation_supported);
  }
}

void AudioEffectsController::AddNoiseCancellationEffect() {
  const auto noise_cancellation_id = VcEffectId::kNoiseCancellation;

  // Do nothing if the effect was already added.
  if (IsEffectsAdded(noise_cancellation_id)) {
    return;
  }

  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this), noise_cancellation_id),
      /*effect_id=*/noise_cancellation_id);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kVideoConferenceNoiseCancellationOnIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/noise_cancellation_id,
                          /*value=*/0));
  effect->AddState(std::move(effect_state));

  effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kMicrophone);
  AddEffect(std::move(effect));

  // Register this delegate if needed so that the effect is added to the UI.
  // Note that other functions might register this delegate already and we need
  // to avoid registering twice.
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();
  if (!effects_manager.IsDelegateRegistered(this)) {
    effects_manager.RegisterDelegate(this);
  }
}

void AudioEffectsController::AddStyleTransferEffect() {
  const auto style_transfer_id = VcEffectId::kStyleTransfer;

  // Do nothing if the effect was already added.
  if (IsEffectsAdded(style_transfer_id)) {
    return;
  }

  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this), style_transfer_id),
      /*effect_id=*/style_transfer_id);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kUnifiedMenuMicStyleTransferIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUDIO_INPUT_STYLE_TRANSFER),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_STYLE_TRANSFER,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/style_transfer_id,
                          /*value=*/0));
  effect->AddState(std::move(effect_state));

  effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kMicrophone);
  AddEffect(std::move(effect));

  // Register this delegate if needed so that the effect is added to the UI.
  // Note that other functions might register this delegate already and we need
  // to avoid registering twice.
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();
  if (!effects_manager.IsDelegateRegistered(this)) {
    effects_manager.RegisterDelegate(this);
  }
}

void AudioEffectsController::AddLiveCaptionEffect() {
  const auto live_caption_id = VcEffectId::kLiveCaption;

  // Do nothing if the effect was already added.
  if (IsEffectsAdded(live_caption_id)) {
    return;
  }

  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this), live_caption_id),
      /*effect_id=*/live_caption_id);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kVideoConferenceLiveCaptionOnIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/live_caption_id,
                          /*value=*/0));

  effect->AddState(std::move(effect_state));
  AddEffect(std::move(effect));

  // Register this delegate if needed so that the effect is added to the UI.
  // Note that other functions might register this delegate already and we need
  // to avoid registering twice.
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();
  if (!effects_manager.IsDelegateRegistered(this)) {
    effects_manager.RegisterDelegate(this);
  }
}

bool AudioEffectsController::IsEffectsAdded(VcEffectId id) {
  return GetEffectById(id);
}

}  // namespace ash
