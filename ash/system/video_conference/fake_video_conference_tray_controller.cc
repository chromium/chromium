// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/fake_video_conference_tray_controller.h"

#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

FakeVideoConferenceTrayController::FakeVideoConferenceTrayController()
    : effect_repository_(
          std::make_unique<fake_video_conference::EffectRepository>(
              /*controller=*/this)) {}

FakeVideoConferenceTrayController::~FakeVideoConferenceTrayController() {
  effect_repository_.reset();
}

void FakeVideoConferenceTrayController::SetCameraMuted(bool muted) {
  camera_muted_ = muted;
  OnCameraSWPrivacySwitchStateChanged(
      camera_muted_ ? cros::mojom::CameraPrivacySwitchState::ON
                    : cros::mojom::CameraPrivacySwitchState::OFF);
}

void FakeVideoConferenceTrayController::SetMicrophoneMuted(bool muted) {
  microphone_muted_ = muted;
  OnInputMuteChanged(/*mute_on=*/microphone_muted_,
                     CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
}

}  // namespace ash
