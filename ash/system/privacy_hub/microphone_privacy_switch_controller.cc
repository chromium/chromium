// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include <memory>

#include "ash/components/audio/cras_audio_handler.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "components/prefs/pref_service.h"

namespace ash {

MicrophonePrivacySwitchController::MicrophonePrivacySwitchController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

MicrophonePrivacySwitchController::~MicrophonePrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
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
}

void MicrophonePrivacySwitchController::OnPreferenceChanged() {
  SetSystemMute();
}

void MicrophonePrivacySwitchController::SetSystemMute() {
  const bool allowed = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kUserMicrophoneAllowed);
  CrasAudioHandler::Get()->SetInputMute(!allowed);
}

}  // namespace ash
