// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MICROPHONE_MUTE_MICROPHONE_MUTE_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_MICROPHONE_MUTE_MICROPHONE_MUTE_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/components/audio/cras_audio_handler.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"

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
  void MaybeShowNotification();

  // ash::CrasAudioHandler::AudioObserver:
  void OnInputMuteChanged(bool mute_on) override;
  void OnNumberOfInputStreamsWithPermissionChanged() override;

 private:
  friend class MicrophoneMuteNotificationControllerTest;

  // Creates a notification for telling the user they're attempting to use the
  // mic while the mis is muted.
  void GenerateMicrophoneMuteNotification(
      const absl::optional<std::u16string>& app_name);

  // Mic mute notification title.
  std::u16string GetNotificationTitle() const;

  // Mic mute notification body.
  std::u16string GetNotificationMessage(
      const absl::optional<std::u16string>& app_name) const;

  // Takes down the mic mute notification.
  void RemoveMicrophoneMuteNotification();

  // Returns true if we have any active input stream with permission, of any
  // client type.  See
  // ash::CrasAudioClient::NumberOfInputStreamsWithPermissionChanged() for more
  // details.
  bool HaveActiveInputStreams();

  static const char kNotificationId[];

  // A value of true means the mic is muted.
  bool mic_mute_on_ = false;

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
