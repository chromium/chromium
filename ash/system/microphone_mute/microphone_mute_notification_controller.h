// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MICROPHONE_MUTE_MICROPHONE_MUTE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_MICROPHONE_MUTE_MICROPHONE_MUTE_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace message_center {
class Notification;
}

namespace ash {

// Controller class to manage microphone mute notifications. This
// notification shows up when the user launches an app that uses the microphone
// while the microphone is muted.
class ASH_EXPORT MicrophoneMuteNotificationController
    : public ash::CrasAudioHandler::AudioObserver {
 public:
  MicrophoneMuteNotificationController();
  MicrophoneMuteNotificationController(
      const MicrophoneMuteNotificationController&) = delete;
  MicrophoneMuteNotificationController& operator=(
      const MicrophoneMuteNotificationController&) = delete;
  ~MicrophoneMuteNotificationController() override;

  // Shows the microphone muted notification if it needs to be shown.
  // |priority| - The priority with which the notification should be shown.
  // |recreate| - Whether the notification should be recreated if it's already
  // shown.
  void MaybeShowNotification(message_center::NotificationPriority priority,
                             bool recreate);

  // ash::CrasAudioHandler::AudioObserver:
  void OnInputMuteChanged(bool mute_on) override;
  void OnInputMutedByMicrophoneMuteSwitchChanged(bool muted) override;
  void OnNumberOfInputStreamsWithPermissionChanged() override;

 private:
  friend class MicrophoneMuteNotificationControllerTest;

  // Creates a notification for telling the user they're attempting to use the
  // mic while the mic is muted.
  std::unique_ptr<message_center::Notification>
  GenerateMicrophoneMuteNotification(
      const absl::optional<std::u16string>& app_name,
      message_center::NotificationPriority priority);

  // Mic mute notification title.
  std::u16string GetNotificationTitle(
      const absl::optional<std::u16string>& app_name) const;

  // Mic mute notification body.
  std::u16string GetNotificationMessage() const;

  // Takes down the mic mute notification.
  void RemoveMicrophoneMuteNotification();

  // Returns number if we have any active input stream with permission, of any
  // client type.  See
  // ash::CrasAudioClient::NumberOfInputStreamsWithPermissionChanged() for more
  // details.
  int CountActiveInputStreams();

  static const char kNotificationId[];

  // Whether the microphone is muted.
  bool mic_mute_on_ = false;
  // Whether the microphone is muted using a microphone mute switch.
  bool mic_muted_by_mute_switch_ = false;
  // The number of currently active audio input steams.
  int input_stream_count_ = 0;

  // Set when a microphone mute notification is shown. Contains the notification
  // priority used for the notification.
  absl::optional<message_center::NotificationPriority>
      current_notification_priority_;

  base::ScopedObservation<ash::CrasAudioHandler,
                          AudioObserver,
                          &ash::CrasAudioHandler::AddAudioObserver,
                          &ash::CrasAudioHandler::RemoveAudioObserver>
      audio_observation_{this};

  base::WeakPtrFactory<MicrophoneMuteNotificationController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MICROPHONE_MUTE_MICROPHONE_MUTE_NOTIFICATION_CONTROLLER_H_
