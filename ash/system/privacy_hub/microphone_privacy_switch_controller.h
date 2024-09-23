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

namespace ash {

// This controller keeps the `kUserMicrophoneAllowed` preference and the state
// of the system input mute in sync.
class ASH_EXPORT MicrophonePrivacySwitchController
    : public CrasAudioHandler::AudioObserver,
      public SessionObserver {
 public:
  MicrophonePrivacySwitchController();
  MicrophonePrivacySwitchController(const MicrophonePrivacySwitchController&) =
      delete;
  MicrophonePrivacySwitchController& operator=(
      const MicrophonePrivacySwitchController&) = delete;
  ~MicrophonePrivacySwitchController() override;

  static MicrophonePrivacySwitchController* Get();

  // CrasAudioHandler::AudioObserver
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;
  void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) override;
  void OnNumberOfInputStreamsWithPermissionChanged() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // Returns false if the microphone is globally blocked by the OS level switch.
  bool IsMicrophoneUsageAllowed() const;

 private:
  // A callback that is invoked when the user changes `kUserMicrophoneAllowed`
  // preference from the Privacy Hub UI.
  void OnPreferenceChanged();

  // Updates the microphone mute status according to the user preference.
  void SetSystemMute();

  // Show/hide the appropriate notification for the current state of the system.
  // This means showing HW notification before even considering if a SW
  // notification should be shown.
  void SetMicrophoneNotificationVisible(bool visible);

  // Silently updates the message of the exiting microphone mute notification
  // with the appropriate application names(s).
  void UpdateMicrophoneNotification();

  size_t input_stream_count_ = 0;
  bool mic_mute_on_ = false;
  bool mic_muted_by_mute_switch_ = false;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_MICROPHONE_PRIVACY_SWITCH_CONTROLLER_H_
