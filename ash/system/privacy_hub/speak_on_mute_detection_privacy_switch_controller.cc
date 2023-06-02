// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/speak_on_mute_detection_privacy_switch_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

SpeakOnMuteDetectionPrivacySwitchController::
    SpeakOnMuteDetectionPrivacySwitchController() {
  // Resets the speak-on-mute detection state in CRAS as CRAS is not restarted
  // when chrome restarts.
  CrasAudioHandler::Get()->SetSpeakOnMuteDetection(/*som_on=*/false);

  // Only observes `SessionController` if the feature is enabled.
  if (!features::IsVideoConferenceEnabled()) {
    return;
  }
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  session_observation_.Observe(session_controller);
}

SpeakOnMuteDetectionPrivacySwitchController::
    ~SpeakOnMuteDetectionPrivacySwitchController() = default;

void SpeakOnMuteDetectionPrivacySwitchController::
    OnActiveUserPrefServiceChanged(PrefService* pref_service) {
  // Subscribes again to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserSpeakOnMuteDetectionEnabled,
      base::BindRepeating(&SpeakOnMuteDetectionPrivacySwitchController::
                              SetSpeakOnMuteDetectionFromPref,
                          base::Unretained(this)));
  // Initializes again the speak-on-mute detection local variable.
  speak_on_mute_detection_pref_on_ =
      pref_change_registrar_->prefs()->GetBoolean(
          prefs::kUserSpeakOnMuteDetectionEnabled);

  // Manually sets the speak-on-mute detection state to the value of the user
  // preference when creating the controller during the browser initialization
  // after creating the user profile.
  SetSpeakOnMuteDetectionFromPref();
}

void SpeakOnMuteDetectionPrivacySwitchController::
    SetSpeakOnMuteDetectionFromPref() {
  const bool speak_on_mute_detection_enabled =
      pref_change_registrar_->prefs()->GetBoolean(
          prefs::kUserSpeakOnMuteDetectionEnabled);

  if (speak_on_mute_detection_pref_on_ != speak_on_mute_detection_enabled) {
    speak_on_mute_detection_pref_on_ = speak_on_mute_detection_enabled;
    // No longer shows the opt-in nudge as the speak-on-mute detection pref has
    // changed.
    pref_change_registrar_->prefs()->SetBoolean(
        prefs::kShouldShowSpeakOnMuteOptInNudge, false);
  }

  // Always sends profile state to the CRAS as it can be different from the
  // local state.
  CrasAudioHandler::Get()->SetSpeakOnMuteDetection(
      speak_on_mute_detection_pref_on_);
}

}  // namespace ash
