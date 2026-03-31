// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/fake_video_conference_tray_controller.h"

#include <vector>

#include "ash/system/video_conference/effects/fake_video_conference_effects.h"
#include "ash/system/video_conference/effects/fake_video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "url/gurl.h"

namespace ash {

namespace {

VideoConferenceMediaAppInfo CreateMediaApp(const base::UnguessableToken& id,
                                           base::Time last_activity_time,
                                           bool is_capturing_camera,
                                           bool is_capturing_microphone,
                                           bool is_capturing_screen,
                                           const std::u16string& title,
                                           std::optional<GURL> url,
                                           VideoConferenceAppType app_type) {
  VideoConferenceMediaAppInfo app;
  app.id = id;
  app.last_activity_time = last_activity_time;
  app.is_capturing_camera = is_capturing_camera;
  app.is_capturing_microphone = is_capturing_microphone;
  app.is_capturing_screen = is_capturing_screen;
  app.title = title;
  app.url = std::move(url);
  app.app_type = app_type;
  return app;
}

}  // namespace

FakeVideoConferenceTrayController::FakeVideoConferenceTrayController()
    : effect_repository_(
          std::make_unique<fake_video_conference::EffectRepository>(
              /*controller=*/this)) {
  AddMediaApp(CreateMediaApp(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(),
      /*is_capturing_camera=*/true, /*is_capturing_microphone=*/false,
      /*is_capturing_screen=*/false, /*title=*/u"Google Meet",
      /*url=*/GURL("https://meet.google.com/abc-xyz/ab-123"),
      /*app_type=*/VideoConferenceAppType::kChromeTab));
  AddMediaApp(CreateMediaApp(
      /*id=*/base::UnguessableToken::Create(),
      /*last_activity_time=*/base::Time::Now(),
      /*is_capturing_camera=*/false, /*is_capturing_microphone=*/true,
      /*is_capturing_screen=*/true, /*title=*/u"Zoom",
      /*url=*/std::nullopt, /*app_type=*/VideoConferenceAppType::kChromeTab));
}

FakeVideoConferenceTrayController::~FakeVideoConferenceTrayController() {
  effect_repository_.reset();
}

void FakeVideoConferenceTrayController::SetCameraMuted(bool muted) {
  OnCameraSWPrivacySwitchStateChanged(
      muted ? cros::mojom::CameraPrivacySwitchState::ON
            : cros::mojom::CameraPrivacySwitchState::OFF);
}

void FakeVideoConferenceTrayController::SetMicrophoneMuted(bool muted) {
  microphone_muted_ = muted;
  OnInputMuteChanged(/*mute_on=*/microphone_muted_,
                     CrasAudioHandler::InputMuteChangeMethod::kKeyboardButton);
}

bool FakeVideoConferenceTrayController::GetCameraMuted() {
  return camera_muted_by_hardware_switch() || camera_muted_by_software_switch();
}

bool FakeVideoConferenceTrayController::GetMicrophoneMuted() {
  return microphone_muted_;
}

VideoConferenceTrayEffectsManager&
FakeVideoConferenceTrayController::GetEffectsManager() {
  return effects_manager_ ? *effects_manager_
                          : VideoConferenceTrayController::GetEffectsManager();
}

void FakeVideoConferenceTrayController::SetEffectsManager(
    VideoConferenceTrayEffectsManager* effects_manager) {
  effects_manager_ = effects_manager;
}

void FakeVideoConferenceTrayController::GetMediaApps(
    base::OnceCallback<void(MediaApps)> ui_callback) {
  // If initialized, use real VideoConferenceTrayController for GetMediaApps.
  if (initialized()) {
    VideoConferenceTrayController::GetMediaApps(std::move(ui_callback));
    return;
  }

  // If not initialized, use fake `media_apps_`.
  std::move(ui_callback).Run(media_apps_);
}

void FakeVideoConferenceTrayController::ReturnToApp(
    const base::UnguessableToken& id) {
  app_to_launch_state_[id] = true;

  // Call real ReturnToApp if initialized.
  if (initialized()) {
    VideoConferenceTrayController::ReturnToApp(id);
  }
}

void FakeVideoConferenceTrayController::HandleDeviceUsedWhileDisabled(
    VideoConferenceMediaDevice device,
    const std::u16string& app_name) {
  VideoConferenceTrayController::HandleDeviceUsedWhileDisabled(device,
                                                               app_name);
  device_used_while_disabled_records_.emplace_back(device, app_name);
}

void FakeVideoConferenceTrayController::HandleClientUpdate(
    VideoConferenceClientUpdate update) {
  last_client_update_ = std::move(update);
}

void FakeVideoConferenceTrayController::AddMediaApp(
    VideoConferenceMediaAppInfo media_app) {
  media_apps_.push_back(std::move(media_app));
}

void FakeVideoConferenceTrayController::ClearMediaApps() {
  media_apps_.clear();
}

}  // namespace ash
