// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_effects_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
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

AudioEffectsController::AudioEffectsController() {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);
}

AudioEffectsController::~AudioEffectsController() {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->effects_manager();
  if (effects_manager.IsDelegateRegistered(this)) {
    effects_manager.UnregisterDelegate(this);
  }
}

bool AudioEffectsController::IsEffectSupported(
    AudioEffectId effect_id /*=AudioEffectId::kNone*/) {
  switch (effect_id) {
    case AudioEffectId::kNoiseCancellation:
      return CrasAudioHandler::Get()->noise_cancellation_supported();
    case AudioEffectId::kLiveCaption:
      return captions::IsLiveCaptionFeatureSupported();
    case AudioEffectId::kNone:
      return IsEffectSupported(AudioEffectId::kNoiseCancellation) ||
             IsEffectSupported(AudioEffectId::kLiveCaption);
  }

  NOTREACHED();
  return false;
}

absl::optional<int> AudioEffectsController::GetEffectState(int effect_id) {
  switch (effect_id) {
    case AudioEffectId::kNoiseCancellation: {
      return CrasAudioHandler::Get()->GetNoiseCancellationState() ? 1 : 0;
    }
    case AudioEffectId::kLiveCaption: {
      return Shell::Get()->accessibility_controller()->live_caption().enabled()
                 ? 1
                 : 0;
    }
    case AudioEffectId::kNone:
      return absl::nullopt;
  }

  NOTREACHED();
  return absl::nullopt;
}

void AudioEffectsController::OnEffectControlActivated(
    absl::optional<int> effect_id,
    absl::optional<int> value) {
  DCHECK(effect_id.has_value());
  switch (effect_id.value()) {
    case AudioEffectId::kNoiseCancellation: {
      // Toggle noise cancellation.
      CrasAudioHandler* audio_handler = CrasAudioHandler::Get();
      bool new_state = !audio_handler->GetNoiseCancellationState();
      audio_handler->SetNoiseCancellationState(new_state);
      return;
    }
    case AudioEffectId::kLiveCaption: {
      // Toggle live caption.
      AccessibilityControllerImpl* controller =
          Shell::Get()->accessibility_controller();
      controller->live_caption().SetEnabled(
          !controller->live_caption().enabled());
      return;
    }
    case AudioEffectId::kNone:
      return;
  }

  NOTREACHED();
}

void AudioEffectsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->effects_manager();

  // Invoked when the user initially logs in and on user switching in
  // multi-profile. If the delegate is already registered, no need to continue.
  if (effects_manager.IsDelegateRegistered(this)) {
    return;
  }

  if (IsEffectSupported(AudioEffectId::kNoiseCancellation)) {
    AddNoiseCancellationEffect();
  }

  if (IsEffectSupported(AudioEffectId::kLiveCaption)) {
    AddLiveCaptionEffect();
  }

  if (IsEffectSupported()) {
    effects_manager.RegisterDelegate(this);
  }
}

void AudioEffectsController::AddNoiseCancellationEffect() {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      VcEffectType::kToggle,
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this),
                          static_cast<int>(AudioEffectId::kNoiseCancellation)));
  effect->set_id(static_cast<int>(AudioEffectId::kNoiseCancellation));
  effect->AddState(std::make_unique<VcEffectState>(
      /*icon=*/&kPrivacyIndicatorsMicrophoneIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_AUDIO_INPUT_NOISE_CANCELLATION,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/
                          static_cast<int>(AudioEffectId::kNoiseCancellation),
                          /*value=*/0)));
  AddEffect(std::move(effect));
}

void AudioEffectsController::AddLiveCaptionEffect() {
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      VcEffectType::kToggle,
      base::BindRepeating(&AudioEffectsController::GetEffectState,
                          base::Unretained(this),
                          static_cast<int>(AudioEffectId::kLiveCaption)));
  effect->set_id(static_cast<int>(AudioEffectId::kLiveCaption));
  effect->AddState(std::make_unique<VcEffectState>(
      /*icon=*/&kPrivacyIndicatorsMicrophoneIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LIVE_CAPTION),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_LIVE_CAPTION,
      /*button_callback=*/
      base::BindRepeating(&AudioEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/
                          static_cast<int>(AudioEffectId::kLiveCaption),
                          /*value=*/0)));
  AddEffect(std::move(effect));
}

}  // namespace ash
