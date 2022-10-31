// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_MICROPHONE_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_MICROPHONE_PRIVACY_SWITCH_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/events/devices/microphone_mute_switch_monitor.h"

namespace ash {

// This controller keeps the KUserMicrophoneAllowed preference and the state of
// the system input mute in sync.
class ASH_EXPORT MicrophonePrivacySwitchController
    : public ui::MicrophoneMuteSwitchMonitor::Observer,
      public CrasAudioHandler::AudioObserver,
      public SessionObserver {
 public:
  MicrophonePrivacySwitchController();
  ~MicrophonePrivacySwitchController() override;

  MicrophonePrivacySwitchController(const MicrophonePrivacySwitchController&) =
      delete;
  MicrophonePrivacySwitchController& operator=(
      const MicrophonePrivacySwitchController&) = delete;

  // SessionObserver
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // CrasAudioHandler::AudioObserver
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;

  // CrasAudioHandler::AudioObserver
  void OnAudioNodesChanged() override;

  // ui::MicrophoneMuteSwitchMonitor::Observer
  void OnMicrophoneMuteSwitchValueChanged(bool muted) override;

 private:
  // A callback that is invoked when the user changes KUserMicrophoneAllowed
  // preference from the Privacy Hub UI.
  void OnPreferenceChanged();

  // Updates the microphone mute status according to the user preference.
  void SetSystemMute();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_MICROPHONE_PRIVACY_SWITCH_CONTROLLER_H_
