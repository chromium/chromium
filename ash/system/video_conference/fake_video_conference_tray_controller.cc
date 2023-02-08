// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/fake_video_conference_tray_controller.h"

#include <vector>

#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "url/gurl.h"

namespace ash {

FakeVideoConferenceTrayController::FakeVideoConferenceTrayController()
    : effect_repository_(
          std::make_unique<fake_video_conference::EffectRepository>(
              /*controller=*/this)) {
  AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(),
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/GURL("https://meet.google.com/abc-xyz/ab-123")));
  AddMediaApp(crosapi::mojom::VideoConferenceMediaAppInfo::New(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(),
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/absl::nullopt));
}

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

bool FakeVideoConferenceTrayController::GetCameraMuted() {
  return camera_muted();
}

bool FakeVideoConferenceTrayController::GetMicrophoneMuted() {
  return microphone_muted();
}

void FakeVideoConferenceTrayController::GetMediaApps(
    base::OnceCallback<void(MediaApps)> ui_callback) {
  MediaApps apps;
  for (auto& app : media_apps_) {
    apps.push_back(app->Clone());
  }
  std::move(ui_callback).Run(std::move(apps));
}

void FakeVideoConferenceTrayController::ReturnToApp(
    const base::UnguessableToken& id) {
  app_to_launch_state_[id] = true;
}

void FakeVideoConferenceTrayController::HandleDeviceUsedWhileDisabled(
    crosapi::mojom::VideoConferenceMediaDevice device,
    const std::u16string& app_name) {
  device_used_while_disabled_records_.emplace_back(device, app_name);
}

void FakeVideoConferenceTrayController::AddMediaApp(
    crosapi::mojom::VideoConferenceMediaAppInfoPtr media_app) {
  media_apps_.push_back(std::move(media_app));
}

void FakeVideoConferenceTrayController::ClearMediaApps() {
  media_apps_.clear();
}

}  // namespace ash
