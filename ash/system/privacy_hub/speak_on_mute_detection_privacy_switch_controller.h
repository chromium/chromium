// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_PRIVACY_SWITCH_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/scoped_observation.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {

// This controller keeps the `kUserSpeakOnMuteDetectionEnabled` preference and
// the state of the speak-on-mute detection in sync.
class ASH_EXPORT SpeakOnMuteDetectionPrivacySwitchController
    : public SessionObserver {
 public:
  SpeakOnMuteDetectionPrivacySwitchController();
  SpeakOnMuteDetectionPrivacySwitchController(
      const SpeakOnMuteDetectionPrivacySwitchController&) = delete;
  SpeakOnMuteDetectionPrivacySwitchController& operator=(
      const SpeakOnMuteDetectionPrivacySwitchController&) = delete;
  ~SpeakOnMuteDetectionPrivacySwitchController() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

 private:
  // Enables/disables the speak-on-mute detection according to the user
  // preference.
  void SetSpeakOnMuteDetectionFromPref();

  // The profile pref state of the speak-on-mute detection.
  bool speak_on_mute_detection_pref_on_ = false;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_SPEAK_ON_MUTE_DETECTION_PRIVACY_SWITCH_CONTROLLER_H_
