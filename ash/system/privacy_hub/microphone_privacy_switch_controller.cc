// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_service.h"

namespace ash {

MicrophonePrivacySwitchController::MicrophonePrivacySwitchController() {
  Shell::Get()->session_controller()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

MicrophonePrivacySwitchController::~MicrophonePrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

void MicrophonePrivacySwitchController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Subscribing again to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserMicrophoneAllowed,
      base::BindRepeating(
          &MicrophonePrivacySwitchController::OnPreferenceChanged,
          base::Unretained(this)));
  // Manually set the system input mute state to the value of the user
  // preference when creating the controller during the browser initialization
  // after creating the user profile.
  SetSystemMute();
}

void MicrophonePrivacySwitchController::OnInputMuteChanged(bool mute_on) {
  // `pref_change_registrar_` is only initialized after a user logs in.
  if (pref_change_registrar_ == nullptr) {
    return;
  }

  PrefService* prefs = pref_change_registrar_->prefs();
  DCHECK(prefs);

  const bool microphone_allowed = !mute_on;
  if (prefs->GetBoolean(prefs::kUserMicrophoneAllowed) != microphone_allowed) {
    prefs->SetBoolean(prefs::kUserMicrophoneAllowed, microphone_allowed);
  }
}

void MicrophonePrivacySwitchController::OnPreferenceChanged() {
  SetSystemMute();
}

void MicrophonePrivacySwitchController::SetSystemMute() {
  DCHECK(pref_change_registrar_);

  const bool microphone_allowed = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kUserMicrophoneAllowed);
  const bool microphone_muted = !microphone_allowed;
  if (CrasAudioHandler::Get()->IsInputMuted() != microphone_muted) {
    CrasAudioHandler::Get()->SetInputMute(microphone_muted);
  }
}

}  // namespace ash
