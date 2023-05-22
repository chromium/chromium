// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {
// The ID for the "Speak-on-mute detected" nudge.
constexpr char kVideoConferenceTraySpeakOnMuteDetectedNudgeId[] =
    "video_conference_tray_nudge_ids.speak_on_mute_detected";

// The ID for the "use while disabled" nudge.
constexpr char kVideoConferenceTrayUseWhileDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.use_while_disabled";

// The cool down duration for speak-on-mute detection notification in seconds.
constexpr int KSpeakOnMuteNotificationCoolDownDuration = 60;

VideoConferenceTrayController* g_controller_instance = nullptr;

bool IsAnyShelfAutoHidden() {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    CHECK(root_window_controller);
    if (root_window_controller->shelf()->auto_hide_behavior() ==
        ShelfAutoHideBehavior::kAlways) {
      return true;
    }
  }
  return false;
}

VideoConferenceTray* GetVcTrayInActiveWindow() {
  return RootWindowController::ForWindow(
             Shell::Get()->GetRootWindowForNewWindows())
      ->GetStatusAreaWidget()
      ->video_conference_tray();
}

}  // namespace

VideoConferenceTrayController::VideoConferenceTrayController() {
  DCHECK(!g_controller_instance);
  g_controller_instance = this;
}

VideoConferenceTrayController::~VideoConferenceTrayController() {
  DCHECK_EQ(this, g_controller_instance);
  g_controller_instance = nullptr;

  if (initialized_) {
    media::CameraHalDispatcherImpl::GetInstance()
        ->RemoveCameraPrivacySwitchObserver(this);
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
  }
}

// static
VideoConferenceTrayController* VideoConferenceTrayController::Get() {
  return g_controller_instance;
}

void VideoConferenceTrayController::Initialize(
    VideoConferenceManagerBase* video_conference_manager) {
  DCHECK(!video_conference_manager_)
      << "VideoConferenceTrayController should not be Initialized twice.";
  video_conference_manager_ = video_conference_manager;
  media::CameraHalDispatcherImpl::GetInstance()->AddCameraPrivacySwitchObserver(
      this);
  CrasAudioHandler::Get()->AddAudioObserver(this);
  initialized_ = true;
}

void VideoConferenceTrayController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VideoConferenceTrayController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool VideoConferenceTrayController::ShouldShowTray() const {
  DCHECK(Shell::Get());

  // We only show the tray in an active session and if there's a media app
  // running.
  return Shell::Get()->session_controller()->GetSessionState() ==
             session_manager::SessionState::ACTIVE &&
         state_.has_media_app;
}

bool VideoConferenceTrayController::GetHasCameraPermissions() const {
  return state_.has_camera_permission;
}

bool VideoConferenceTrayController::GetHasMicrophonePermissions() const {
  return state_.has_microphone_permission;
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

void VideoConferenceTrayController::SetCameraMuted(bool muted) {
  // If the camera is hardware-muted, do nothing here.
  if (camera_muted_by_hardware_switch_) {
    // TODO(b/272145024): Display a nudge if camera button is clicked during
    // hardware-muted.
    return;
  }

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

bool VideoConferenceTrayController::GetCameraMuted() {
  if (camera_muted_by_hardware_switch_) {
    return true;
  }

  if (!features::IsCrosPrivacyHubEnabled()) {
    return camera_muted_by_software_switch_;
  }

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref_service && !pref_service->GetBoolean(prefs::kUserCameraAllowed);
}

void VideoConferenceTrayController::SetMicrophoneMuted(bool muted) {
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

bool VideoConferenceTrayController::GetMicrophoneMuted() {
  if (!features::IsCrosPrivacyHubEnabled()) {
    return CrasAudioHandler::Get()->IsInputMuted();
  }

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref_service &&
         !pref_service->GetBoolean(prefs::kUserMicrophoneAllowed);
}

void VideoConferenceTrayController::GetMediaApps(
    base::OnceCallback<void(MediaApps)> ui_callback) {
  DCHECK(video_conference_manager_);
  video_conference_manager_->GetMediaApps(std::move(ui_callback));
}

void VideoConferenceTrayController::ReturnToApp(
    const base::UnguessableToken& id) {
  DCHECK(video_conference_manager_);
  video_conference_manager_->ReturnToApp(id);
}

void VideoConferenceTrayController::OnCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
    cros::mojom::CameraPrivacySwitchState state) {
  camera_muted_by_hardware_switch_ =
      state == cros::mojom::CameraPrivacySwitchState::ON;

  UpdateCameraIcons();

  if (video_conference_manager_) {
    video_conference_manager_->SetSystemMediaDeviceStatus(
        crosapi::mojom::VideoConferenceMediaDevice::kCamera,
        /*disabled=*/GetCameraMuted());
  }
}

void VideoConferenceTrayController::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  camera_muted_by_software_switch_ =
      state == cros::mojom::CameraPrivacySwitchState::ON;

  UpdateCameraIcons();

  if (video_conference_manager_) {
    video_conference_manager_->SetSystemMediaDeviceStatus(
        crosapi::mojom::VideoConferenceMediaDevice::kCamera,
        /*disabled=*/GetCameraMuted());
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

  if (video_conference_manager_) {
    video_conference_manager_->SetSystemMediaDeviceStatus(
        crosapi::mojom::VideoConferenceMediaDevice::kMicrophone,
        /*disabled=*/mute_on);
  }

  microphone_muted_by_hardware_switch_ =
      method == CrasAudioHandler::InputMuteChangeMethod::kPhysicalShutter;

  // Reset the speak-on-mute notification timer when change to mute so user can
  // get instant speak-on-mute notification when they mute their microphone.
  if (mute_on) {
    last_speak_on_mute_notification_time_.reset();
  }
}

void VideoConferenceTrayController::OnSpeakOnMuteDetected() {
  const base::TimeTicks current_time = base::TimeTicks::Now();

  if (!last_speak_on_mute_notification_time_.has_value() ||
      (current_time - last_speak_on_mute_notification_time_.value())
              .InSeconds() >= KSpeakOnMuteNotificationCoolDownDuration) {
    AnchoredNudgeData nudge_data(
        kVideoConferenceTraySpeakOnMuteDetectedNudgeId,
        AnchoredNudgeCatalogName::kVideoConferenceTraySpeakOnMuteDetected,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_TOAST_SPEAK_ON_MUTE_DETECTED),
        /*anchor_view=*/GetVcTrayInActiveWindow()->audio_icon());
    AnchoredNudgeManager::Get()->Show(nudge_data);

    last_speak_on_mute_notification_time_.emplace(current_time);
  }
}

base::OneShotTimer&
VideoConferenceTrayController::GetShelfAutoHideTimerForTest() {
  return disable_shelf_autohide_timer_;
}

void VideoConferenceTrayController::UpdateWithMediaState(
    VideoConferenceMediaState state) {
  auto old_state = state_;
  const bool old_tray_target_visibility = ShouldShowTray();
  state_ = state;
  const bool new_tray_target_visibility = ShouldShowTray();

  if (new_tray_target_visibility && !old_tray_target_visibility) {
    effects_manager_.RecordInitialStates();
  }

  if (state_.has_media_app != old_state.has_media_app) {
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

  // If any `Shelf` is auto hidden, request a new list of `MediaApps` because
  // the `Shelf` needs to be forced shown, or allowed to hide, depending on the
  // current number of capturing applications.
  if (!IsAnyShelfAutoHidden()) {
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (!state_.has_media_app) {
    if (disable_shelf_autohide_timer_.IsRunning()) {
      disable_shelf_autohide_timer_.Stop();
    }
    disable_shelf_autohide_locks_.clear();
    return;
  }

  // The `Shelf` may need to be forced shown if a new app has started accessing
  // the sensors, also `UpdateWithMediaState()` may be called with no change to
  // `state_.has_media_app`, and if a new app is capturing the shelf needs to be
  // re-shown.
  GetMediaApps(
      base::BindOnce(&VideoConferenceTrayController::UpdateShelfAutoHide,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool VideoConferenceTrayController::HasCameraPermission() const {
  return state_.has_camera_permission;
}

bool VideoConferenceTrayController::HasMicrophonePermission() const {
  return state_.has_microphone_permission;
}

void VideoConferenceTrayController::HandleDeviceUsedWhileDisabled(
    crosapi::mojom::VideoConferenceMediaDevice device,
    const std::u16string& app_name) {
  // TODO(b/273570886): Handle the case when both camera and microphone are
  // being used while disabled.
  std::u16string device_name;
  int text_id;
  views::View* anchor_view = nullptr;
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      device_name =
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME);
      text_id =
          microphone_muted_by_hardware_switch_
              ? IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED
              : IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED;
      anchor_view = GetVcTrayInActiveWindow()->audio_icon();
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      device_name =
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME);
      text_id =
          camera_muted_by_hardware_switch_
              ? IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED
              : IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED;
      anchor_view = GetVcTrayInActiveWindow()->camera_icon();
      break;
    default:
      NOTREACHED();
      return;
  }

  AnchoredNudgeData nudge_data(
      kVideoConferenceTrayUseWhileDisabledNudgeId,
      AnchoredNudgeCatalogName::kVideoConferenceTrayUseWhileDisabled,
      l10n_util::GetStringFUTF16(text_id, app_name, device_name), anchor_view);
  AnchoredNudgeManager::Get()->Show(nudge_data);
}

void VideoConferenceTrayController::UpdateCameraIcons() {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    auto* camera_icon = root_window_controller->GetStatusAreaWidget()
                            ->video_conference_tray()
                            ->camera_icon();

    camera_icon->SetToggled(camera_muted_by_hardware_switch_ ||
                            camera_muted_by_software_switch_);
    camera_icon->UpdateCapturingState();
  }
}

void VideoConferenceTrayController::UpdateShelfAutoHide(MediaApps media_apps) {
  const int old_capturing_apps = capturing_apps_;
  capturing_apps_ = media_apps.size();

  // Don't force show the `Shelf` if the number of apps accessing the sensors
  // has decreased. Also do not force hide the shelf when the number of
  // capturing apps decreases, that will be done only if the number of capturing
  // apps drops to zero and is handled elsewhere.
  if (old_capturing_apps >= capturing_apps_) {
    return;
  }

  if (disable_shelf_autohide_locks_.empty()) {
    for (auto* root_window_controller :
         Shell::Get()->GetAllRootWindowControllers()) {
      CHECK(root_window_controller);
      CHECK(root_window_controller->shelf());

      disable_shelf_autohide_locks_.emplace_back(
          root_window_controller->shelf());
    }
  }

  disable_shelf_autohide_timer_.Start(
      FROM_HERE, base::Seconds(6),
      base::BindOnce(
          [](std::list<Shelf::ScopedDisableAutoHide>&
                 disable_shelf_autohide_locks) {
            disable_shelf_autohide_locks.clear();
          },
          std::ref(disable_shelf_autohide_locks_)));
}

}  // namespace ash
