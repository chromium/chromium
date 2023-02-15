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
// Delay time of hiding the tray.
constexpr base::TimeDelta kHideTrayDelay = base::Seconds(12);

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

bool VideoConferenceTrayController::ShouldShowTray() const {
  return tray_hide_delay_timer_.IsRunning() ? true : state_.has_media_app;
}

bool VideoConferenceTrayController::GetHasCameraPermissions() const {
  return tray_hide_delay_timer_.IsRunning() ? camera_permission_during_timer_
                                            : state_.has_camera_permission;
}

bool VideoConferenceTrayController::GetHasMicrophonePermissions() const {
  return tray_hide_delay_timer_.IsRunning()
             ? microphone_permission_during_timer_
             : state_.has_microphone_permission;
}

bool VideoConferenceTrayController::IsCapturingScreen() const {
  return state_.is_capturing_screen;
}

bool VideoConferenceTrayController::IsCapturingCamera() const {
  return state_.is_capturing_camera;
}

bool VideoConferenceTrayController::IsCapturingMicrophone() const {
  return state_.is_capturing_microphone;
}

void VideoConferenceTrayController::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  camera_muted_by_software_switch_ =
      state != cros::mojom::CameraPrivacySwitchState::ON;

  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    auto* camera_icon = root_window_controller->GetStatusAreaWidget()
                            ->video_conference_tray()
                            ->camera_icon();

    camera_icon->SetToggled(!camera_muted_by_software_switch_);
    camera_icon->UpdateCapturingState();
  }
}

void VideoConferenceTrayController::OnInputMuteChanged(
    bool mute_on,
    CrasAudioHandler::InputMuteChangeMethod method) {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    auto* audio_icon = root_window_controller->GetStatusAreaWidget()
                           ->video_conference_tray()
                           ->audio_icon();

    audio_icon->SetToggled(mute_on);
    audio_icon->UpdateCapturingState();
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
    // Reset any on-going timer run.
    tray_hide_delay_timer_.Stop();

    if (!state_.has_media_app) {
      camera_permission_during_timer_ = old_state.has_camera_permission;
      microphone_permission_during_timer_ = old_state.has_microphone_permission;

      // Triggers the timer for delay hiding all the trays. Note that this
      // should be called before `OnCameraPermissionStateChange()` and
      // `OnMicrophonePermissionStateChange()`, since in `VideoConferenceTray`
      // we preserve the state of camera/microphone permission for 12s before
      // hiding the tray.
      tray_hide_delay_timer_.Start(
          FROM_HERE, kHideTrayDelay,
          base::BindOnce(&VideoConferenceTrayController::
                             SetTraysVisibilityAfterDelayHiding,
                         weak_ptr_factory_.GetWeakPtr()));
    }

    for (auto& observer : observer_list_) {
      observer.OnHasMediaAppStateChange();
    }
  }

  if (state_.has_camera_permission != old_state.has_camera_permission) {
    for (auto& observer : observer_list_) {
      observer.OnCameraPermissionStateChange();
    }
  }

  if (state_.has_microphone_permission != old_state.has_microphone_permission) {
    for (auto& observer : observer_list_) {
      observer.OnMicrophonePermissionStateChange();
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

void VideoConferenceTrayController::SetTraysVisibilityAfterDelayHiding() {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    auto* tray =
        root_window_controller->GetStatusAreaWidget()->video_conference_tray();

    DCHECK(tray);
    tray->UpdateTrayAndIconsState();
  }
}

}  // namespace ash
