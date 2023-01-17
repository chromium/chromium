// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/video_conference/video_conference_media_state.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {
// The ID for the "Speak-on-mute detected" toast.
constexpr char kVideoConferenceTraySpeakOnMuteDetectedId[] =
    "video_conference_tray_toast_ids.speak_on_mute_detected";

// The cool down duration for speak-on-mute detection notification in seconds.
constexpr int KSpeakOnMuteNotificationCoolDownDuration = 60;

VideoConferenceTrayController* g_controller_instance = nullptr;
}  // namespace

VideoConferenceTrayController::VideoConferenceTrayController() {
  DCHECK(!g_controller_instance);
  g_controller_instance = this;

  media::CameraHalDispatcherImpl::GetInstance()->AddCameraPrivacySwitchObserver(
      this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
}

VideoConferenceTrayController::~VideoConferenceTrayController() {
  DCHECK_EQ(this, g_controller_instance);
  g_controller_instance = nullptr;

  media::CameraHalDispatcherImpl::GetInstance()
      ->RemoveCameraPrivacySwitchObserver(this);
  CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

// static
VideoConferenceTrayController* VideoConferenceTrayController::Get() {
  return g_controller_instance;
}

void VideoConferenceTrayController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VideoConferenceTrayController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VideoConferenceTrayController::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->video_conference_tray()
        ->camera_icon()
        ->SetToggled(state == cros::mojom::CameraPrivacySwitchState::ON);
  }
}

void VideoConferenceTrayController::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->video_conference_tray()
        ->audio_icon()
        ->SetToggled(mute_on);
  }
}

void VideoConferenceTrayController::OnSpeakOnMuteDetected() {
  const base::TimeTicks current_time = base::TimeTicks::Now();

  if (!last_speak_on_mute_notification_time_.has_value() ||
      (current_time - last_speak_on_mute_notification_time_.value())
              .InSeconds() >= KSpeakOnMuteNotificationCoolDownDuration) {
    ToastData toast_data(
        kVideoConferenceTraySpeakOnMuteDetectedId,
        ToastCatalogName::kVideoConferenceTraySpeakOnMuteDetected,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_TOAST_SPEAK_ON_MUTE_DETECTED),
        ToastData::kDefaultToastDuration,
        /*visible_on_lock_screen=*/false);
    toast_data.show_on_all_root_windows = true;
    ToastManager::Get()->Show(std::move(toast_data));

    last_speak_on_mute_notification_time_.emplace(current_time);
  }
}

void VideoConferenceTrayController::UpdateWithMediaState(
    VideoConferenceMediaState state) {
  auto old_state = state_;
  state_ = state;

  if (state_.has_media_app != old_state.has_media_app) {
    for (auto& observer : observer_list_) {
      observer.OnHasMediaAppStateChange(state_.has_media_app);
    }
  }

  if (state_.has_camera_permission != old_state.has_camera_permission) {
    for (auto& observer : observer_list_) {
      observer.OnCameraPermissionStateChange(state_.has_camera_permission);
    }
  }

  if (state_.has_microphone_permission != old_state.has_microphone_permission) {
    for (auto& observer : observer_list_) {
      observer.OnMicrophonePermissionStateChange(
          state_.has_microphone_permission);
    }
  }

  if (state_.is_capturing_camera != old_state.is_capturing_camera) {
    for (auto& observer : observer_list_)
      observer.OnCameraCapturingStateChange(state_.is_capturing_camera);
  }

  if (state_.is_capturing_microphone != old_state.is_capturing_microphone) {
    for (auto& observer : observer_list_)
      observer.OnMicrophoneCapturingStateChange(state_.is_capturing_microphone);
  }

  if (state_.is_capturing_screen != old_state.is_capturing_screen) {
    for (auto& observer : observer_list_) {
      observer.OnScreenSharingStateChange(state_.is_capturing_screen);
    }
  }
}

void VideoConferenceTrayController::HandleDeviceUsedWhileDisabled(
    crosapi::mojom::VideoConferenceMediaDevice device,
    const std::u16string& app_name) {
  // TODO(b/249828245): Implement logic to handle this.
}

}  // namespace ash
