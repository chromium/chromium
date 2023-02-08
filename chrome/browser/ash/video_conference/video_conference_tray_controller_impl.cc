// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/video_conference/video_conference_tray_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/video_conference/video_conference_manager_ash.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

VideoConferenceTrayControllerImpl::VideoConferenceTrayControllerImpl() =
    default;

VideoConferenceTrayControllerImpl::~VideoConferenceTrayControllerImpl() =
    default;

void VideoConferenceTrayControllerImpl::SetCameraMuted(bool muted) {
  if (!ash::features::IsCrosPrivacyHubEnabled()) {
    media::CameraHalDispatcherImpl::GetInstance()
        ->SetCameraSWPrivacySwitchState(
            muted ? cros::mojom::CameraPrivacySwitchState::ON
                  : cros::mojom::CameraPrivacySwitchState::OFF);
    return;
  }

  // Change user pref to let Privacy Hub enable/disable the camera.
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }
  pref_service->SetBoolean(prefs::kUserCameraAllowed, !muted);
}

bool VideoConferenceTrayControllerImpl::GetCameraMuted() {
  if (!features::IsCrosPrivacyHubEnabled()) {
    return camera_muted_by_software_switch();
  }

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref_service && !pref_service->GetBoolean(prefs::kUserCameraAllowed);
}

void VideoConferenceTrayControllerImpl::SetMicrophoneMuted(bool muted) {
  if (!ash::features::IsCrosPrivacyHubEnabled()) {
    CrasAudioHandler::Get()->SetInputMute(
        /*mute_on=*/muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
    return;
  }

  // Change user pref to let Privacy Hub enable/disable the microphone.
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }
  pref_service->SetBoolean(prefs::kUserMicrophoneAllowed, !muted);
}

bool VideoConferenceTrayControllerImpl::GetMicrophoneMuted() {
  if (!features::IsCrosPrivacyHubEnabled()) {
    return CrasAudioHandler::Get()->IsInputMuted();
  }

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref_service &&
         !pref_service->GetBoolean(prefs::kUserMicrophoneAllowed);
}

void VideoConferenceTrayControllerImpl::GetMediaApps(
    base::OnceCallback<void(MediaApps)> ui_callback) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->GetMediaApps(std::move(ui_callback));
}

void VideoConferenceTrayControllerImpl::ReturnToApp(
    const base::UnguessableToken& id) {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->video_conference_manager_ash()
      ->ReturnToApp(id);
}

}  // namespace ash
