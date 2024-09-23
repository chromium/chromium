// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/system/system_notification_controller.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {
namespace {

size_t CountActiveInputStreams() {
  size_t num_active_streams = 0;
  const base::flat_map<CrasAudioHandler::ClientType, uint32_t> input_streams =
      CrasAudioHandler::Get()->GetNumberOfInputStreamsWithPermission();
  for (const auto& client_type_info : input_streams) {
    num_active_streams += client_type_info.second;
  }

  return num_active_streams;
}

PrivacyHubDelegate* GetFrontend() {
  if (PrivacyHubController* const privacy_hub =
          Shell::Get()->privacy_hub_controller()) {
    return privacy_hub->frontend();
  }
  return nullptr;
}

}  // namespace

MicrophonePrivacySwitchController::MicrophonePrivacySwitchController()
    : input_stream_count_(CountActiveInputStreams()),
      mic_mute_on_(CrasAudioHandler::Get()->IsInputMuted()),
      mic_muted_by_mute_switch_(
          CrasAudioHandler::Get()->input_muted_by_microphone_mute_switch()) {
  Shell::Get()->session_controller()->AddObserver(this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

MicrophonePrivacySwitchController::~MicrophonePrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

// static
MicrophonePrivacySwitchController* MicrophonePrivacySwitchController::Get() {
  auto* privacy_hub_controller = PrivacyHubController::Get();
  return privacy_hub_controller
             ? privacy_hub_controller->microphone_controller()
             : nullptr;
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

bool MicrophonePrivacySwitchController::IsMicrophoneUsageAllowed() const {
  return pref_change_registrar_->prefs()->GetBoolean(
      prefs::kUserMicrophoneAllowed);
}

void MicrophonePrivacySwitchController::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  mic_mute_on_ = mute_on;
  mic_muted_by_mute_switch_ =
      CrasAudioHandler::Get()->input_muted_by_microphone_mute_switch();

  if (!mic_mute_on_) {
    SetMicrophoneNotificationVisible(false);
  }

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

void MicrophonePrivacySwitchController::
    OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) {
  PrivacyHubDelegate* const frontend = GetFrontend();
  if (frontend) {
    // In case this is called before the webui registers a frontend delegate
    frontend->MicrophoneHardwareToggleChanged(muted);
  }

  if (mic_muted_by_mute_switch_ == muted) {
    return;
  }

  mic_muted_by_mute_switch_ = muted;

  if (features::IsVideoConferenceEnabled()) {
    // The `VideoConferenceTrayController` shows this info as a toast.
    return;
  }

  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  if (mic_mute_on_) {
    bool is_mic_sw_switch_notification_shown =
        privacy_hub_notification_controller
            ->IsSoftwareSwitchNotificationDisplayedForSensor(
                SensorDisabledNotificationDelegate::Sensor::kMicrophone);

    if (is_mic_sw_switch_notification_shown) {
      // Set priority to LOW to make sure the notification will be just added
      // to the message center (and not be shown as a popup).
      privacy_hub_notification_controller
          ->SetPriorityForMicrophoneHardwareNotification(
              message_center::NotificationPriority::LOW_PRIORITY);
      SetMicrophoneNotificationVisible(mic_mute_on_);
      // Restore priority to DEFAULT - so next notifications to be popups.
      privacy_hub_notification_controller
          ->SetPriorityForMicrophoneHardwareNotification(
              message_center::NotificationPriority::DEFAULT_PRIORITY);
    }
  } else {
    SetMicrophoneNotificationVisible(false);
  }
}

void MicrophonePrivacySwitchController::
    OnNumberOfInputStreamsWithPermissionChanged() {
  if (features::IsVideoConferenceEnabled()) {
    // The `VideoConferenceTrayController` shows this info as a toast.
    return;
  }
  // Catches the case where a mic-using app is launched while the mic is muted.
  const size_t input_stream_count = CountActiveInputStreams();
  const bool stream_count_increased = input_stream_count > input_stream_count_;
  input_stream_count_ = input_stream_count;

  if (input_stream_count_ == 0) {
    SetMicrophoneNotificationVisible(false);
  } else if (stream_count_increased) {
    SetMicrophoneNotificationVisible(input_stream_count_ && mic_mute_on_);
  } else if (mic_mute_on_) {
    // Microphone is muted and stream count has decreased.
    UpdateMicrophoneNotification();
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
    CrasAudioHandler::Get()->SetInputMute(
        microphone_muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
  }
}

void MicrophonePrivacySwitchController::SetMicrophoneNotificationVisible(
    const bool visible) {
  if (features::IsVideoConferenceEnabled()) {
    // The `VideoConferenceTrayController` shows this info as a toast.
    return;
  }

  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();

  if (visible) {
    if (mic_muted_by_mute_switch_) {
      privacy_hub_notification_controller->ShowHardwareSwitchNotification(
          SensorDisabledNotificationDelegate::Sensor::kMicrophone);
    } else {
      privacy_hub_notification_controller->ShowSoftwareSwitchNotification(
          SensorDisabledNotificationDelegate::Sensor::kMicrophone);
    }

  } else {
    privacy_hub_notification_controller->RemoveSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kMicrophone);
    privacy_hub_notification_controller->RemoveHardwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kMicrophone);
  }
}

void MicrophonePrivacySwitchController::UpdateMicrophoneNotification() {
  if (features::IsVideoConferenceEnabled()) {
    // The `VideoConferenceTrayController` shows this info as a toast.
    return;
  }

  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  if (mic_muted_by_mute_switch_) {
    privacy_hub_notification_controller->UpdateHardwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kMicrophone);
  } else {
    privacy_hub_notification_controller->UpdateSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kMicrophone);
  }
}

}  // namespace ash
