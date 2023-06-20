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
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// The ID for the "Speak-on-mute opt-in" nudge.
constexpr char kVideoConferenceTraySpeakOnMuteOptInNudgeId[] =
    "video_conference_tray_nudge_ids.speak_on_mute_opt_in";

// The ID for the "Speak-on-mute detected" nudge.
constexpr char kVideoConferenceTraySpeakOnMuteDetectedNudgeId[] =
    "video_conference_tray_nudge_ids.speak_on_mute_detected";

// The IDs for the "use while disabled" nudges.
constexpr char kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.microphone_use_while_hw_disabled";
constexpr char kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.microphone_use_while_sw_disabled";
constexpr char kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.camera_use_while_hw_disabled";
constexpr char kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.camera_use_while_sw_disabled";

// VC nudge ids vector that is iterated whenever `CloseAllVcNudges()` is
// called. Please keep in sync whenever adding/removing/updating a nudge id.
const char* const kNudgeIds[] = {
    kVideoConferenceTraySpeakOnMuteOptInNudgeId,
    kVideoConferenceTraySpeakOnMuteDetectedNudgeId,
    kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId,
    kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId,
    kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId,
    kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId};

// The cool down duration for speak-on-mute detection notification in seconds.
constexpr int KSpeakOnMuteNotificationCoolDownDuration = 60;

constexpr auto kRepeatedShowTimerInterval = base::Milliseconds(100);

// The max amount of times the "Speak-on-mute opt-in" nudge can show.
constexpr int kSpeakOnMuteOptInNudgeMaxShownCount = 3;

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

VideoConferenceTrayController::VideoConferenceTrayController()
    : repeated_shows_timer_(
          FROM_HERE,
          kRepeatedShowTimerInterval,
          this,
          &VideoConferenceTrayController::RecordRepeatedShows) {
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
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
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

void VideoConferenceTrayController::MaybeShowSpeakOnMuteOptInNudge(
    VideoConferenceTray* video_conference_tray) {
  // Only attempt to show the speak-on-mute opt-in nudge if the tray is visible
  // preferred in the active display, and microphone input is muted.
  if (!video_conference_tray->visible_preferred() ||
      GetVcTrayInActiveWindow() != video_conference_tray ||
      !GetMicrophoneMuted()) {
    return;
  }

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }

  // The nudge will never be shown again if:
  // - The user has interacted with the nudge before.
  // - The user has toggled on the Speak On Mute feature through settings.
  // - The nudge has been shown its max amount of times.
  if (!pref_service->GetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge)) {
    return;
  }

  // Close all previously shown VC nudges, if any.
  CloseAllVcNudges();

  views::View* anchor_view = GetVcTrayInActiveWindow()->audio_icon();
  if (!anchor_view->GetVisible()) {
    return;
  }

  AnchoredNudgeData nudge_data(
      kVideoConferenceTraySpeakOnMuteOptInNudgeId,
      NudgeCatalogName::kVideoConferenceTraySpeakOnMuteOptIn,
      l10n_util::GetStringUTF16(
          IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_BODY),
      anchor_view);

  nudge_data.title_text = l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_TITLE);

  nudge_data.dismiss_text = l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_DISMISS_BUTTON);
  nudge_data.dismiss_callback = base::BindRepeating(
      &VideoConferenceTrayController::OnSpeakOnMuteNudgeOptOut,
      weak_ptr_factory_.GetWeakPtr());

  nudge_data.second_button_text = l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_SECOND_BUTTON);
  nudge_data.second_button_callback = base::BindRepeating(
      &VideoConferenceTrayController::OnSpeakOnMuteNudgeOptIn,
      weak_ptr_factory_.GetWeakPtr());

  nudge_data.has_infinite_duration = true;

  AnchoredNudgeManager::Get()->Show(nudge_data);

  pref_service->SetInteger(
      prefs::kSpeakOnMuteOptInNudgeShownCount,
      pref_service->GetInteger(prefs::kSpeakOnMuteOptInNudgeShownCount) + 1);

  if (pref_service->GetInteger(prefs::kSpeakOnMuteOptInNudgeShownCount) >=
      kSpeakOnMuteOptInNudgeMaxShownCount) {
    pref_service->SetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge, false);
  }
}

void VideoConferenceTrayController::OnSpeakOnMuteNudgeOptIn() {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }

  pref_service->SetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge, false);
  pref_service->SetBoolean(prefs::kUserSpeakOnMuteDetectionEnabled, true);

  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kVideoConferenceTraySpeakOnMuteOptIn);
}

void VideoConferenceTrayController::OnSpeakOnMuteNudgeOptOut() {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }

  pref_service->SetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge, false);
  pref_service->SetBoolean(prefs::kUserSpeakOnMuteDetectionEnabled, false);

  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kVideoConferenceTraySpeakOnMuteOptIn);
}

void VideoConferenceTrayController::CloseAllVcNudges() {
  for (size_t i = 0; i < std::size(kNudgeIds); ++i) {
    AnchoredNudgeManager::Get()->Cancel(kNudgeIds[i]);
  }
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

  // Attempt recording "Use while disabled" nudge action when camera is unmuted.
  if (!camera_muted_by_hardware_switch_) {
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraUseWhileHWDisabled);
    AnchoredNudgeManager::Get()->Cancel(
        kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId);
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

  // Attempt recording "Use while disabled" nudge action when camera is unmuted.
  if (!camera_muted_by_software_switch_) {
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraUseWhileSWDisabled);
    AnchoredNudgeManager::Get()->Cancel(
        kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId);
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

    // Attempt showing the speak-on-mute opt-in nudge when input is muted.
    MaybeShowSpeakOnMuteOptInNudge(GetVcTrayInActiveWindow());
  } else {
    // Cancel speak-on-mute opt-in nudge if one was being shown.
    AnchoredNudgeManager::Get()->Cancel(
        kVideoConferenceTraySpeakOnMuteOptInNudgeId);

    // Attempt recording "Speak-on-mute" nudge action when mic is unmuted.
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTraySpeakOnMuteDetected);
    AnchoredNudgeManager::Get()->Cancel(
        kVideoConferenceTraySpeakOnMuteDetectedNudgeId);

    // Attempt recording "Use while disabled" nudge action when mic is unmuted.
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
        microphone_muted_by_hardware_switch_
            ? NudgeCatalogName::kVideoConferenceTrayMicrophoneUseWhileHWDisabled
            : NudgeCatalogName::
                  kVideoConferenceTrayMicrophoneUseWhileSWDisabled);
    AnchoredNudgeManager::Get()->Cancel(
        microphone_muted_by_hardware_switch_
            ? kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId
            : kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId);
  }
}

void VideoConferenceTrayController::OnSpeakOnMuteDetected() {
  const base::TimeTicks current_time = base::TimeTicks::Now();

  if (!last_speak_on_mute_notification_time_.has_value() ||
      (current_time - last_speak_on_mute_notification_time_.value())
              .InSeconds() >= KSpeakOnMuteNotificationCoolDownDuration) {
    AnchoredNudgeData nudge_data(
        kVideoConferenceTraySpeakOnMuteDetectedNudgeId,
        NudgeCatalogName::kVideoConferenceTraySpeakOnMuteDetected,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_TOAST_SPEAK_ON_MUTE_DETECTED),
        /*anchor_view=*/GetVcTrayInActiveWindow()->audio_icon());
    // Opens the privacy hub settings page with the mute nudge focused when
    // clicking on the nudge.
    nudge_data.nudge_click_callback = base::BindRepeating([]() -> void {
      Shell::Get()
          ->system_tray_model()
          ->client()
          ->ShowSpeakOnMuteDetectionSettings();
    });
    AnchoredNudgeManager::Get()->Show(nudge_data);

    last_speak_on_mute_notification_time_.emplace(current_time);
  }
}

void VideoConferenceTrayController::OnUserSessionAdded(
    const AccountId& account_id) {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }

  // If enabled, reset the prefs relevant to showing the speak-on-mute opt-in
  // nudge, so it can be shown again for debugging purposes.
  if (features::IsSpeakOnMuteOptInNudgePrefsResetEnabled()) {
    pref_service->SetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge, true);
    pref_service->SetBoolean(prefs::kUserSpeakOnMuteDetectionEnabled, false);
    pref_service->SetInteger(prefs::kSpeakOnMuteOptInNudgeShownCount, 0);
  }
}

void VideoConferenceTrayController::OnShellDestroying() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void VideoConferenceTrayController::HandleClientUpdate(
    crosapi::mojom::VideoConferenceClientUpdatePtr update) {
  // TODO(b/285795457): Implement logic to handle client updates.
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

    // Keeps increment the count to track the number of times the view flickers.
    // When the delay of `kRepeatedShowTimerInterval` has reached, record that
    // count.
    ++count_repeated_shows_;
    repeated_shows_timer_.Reset();
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
    for (auto& observer : observer_list_) {
      observer.OnCameraCapturingStateChange(state_.is_capturing_camera);
    }
  }

  if (state_.is_capturing_microphone != old_state.is_capturing_microphone) {
    for (auto& observer : observer_list_) {
      observer.OnMicrophoneCapturingStateChange(state_.is_capturing_microphone);
    }
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
  NudgeCatalogName catalog_name;
  std::string nudge_id;
  views::View* anchor_view = nullptr;
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      device_name =
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME);
      if (microphone_muted_by_hardware_switch_) {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED;
        nudge_id = kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayMicrophoneUseWhileHWDisabled;
      } else {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED;
        nudge_id = kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayMicrophoneUseWhileSWDisabled;
      }
      anchor_view = GetVcTrayInActiveWindow()->audio_icon();
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      device_name =
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME);
      if (camera_muted_by_hardware_switch_) {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED;
        nudge_id = kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayCameraUseWhileHWDisabled;
      } else {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_SOFTWARE_DISABLED;
        nudge_id = kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayCameraUseWhileSWDisabled;
      }
      anchor_view = GetVcTrayInActiveWindow()->camera_icon();
      break;
    default:
      NOTREACHED();
      return;
  }

  AnchoredNudgeData nudge_data(
      nudge_id, catalog_name,
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

void VideoConferenceTrayController::RecordRepeatedShows() {
  // Note that we also record the metric when `count_repeated_shows_` is one
  // even though this is not a bad signal. This is because we want to record
  // proper shows so we can analyze the repeated shows in context.
  if (count_repeated_shows_ == 0) {
    return;
  }

  base::UmaHistogramCounts100("Ash.VideoConference.NumberOfRepeatedShows",
                              count_repeated_shows_);
  count_repeated_shows_ = 0;
}

}  // namespace ash
