// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/video_conference/video_conference_tray_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
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
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

namespace {

// The ID for the "Speak-on-mute opt-in" nudge.
constexpr char kVideoConferenceTraySpeakOnMuteOptInNudgeId[] =
    "video_conference_tray_nudge_ids.speak_on_mute_opt_in";

// The ID for the "Speak-on-mute opt-in/out confirmation" toast.
constexpr char kVideoConferenceTraySpeakOnMuteOptInConfirmationToastId[] =
    "video_conference_tray_toast_ids.speak_on_mute_opt_in_confirmation";

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
constexpr char kVideoConferenceTrayBothUseWhileDisabledNudgeId[] =
    "video_conference_tray_nudge_ids.camera_microphone_use_while_disabled";

// Boolean prefs used to determine whether to show the gradient animation on the
// buttons. When the value is false, it means that we haved showed the animation
// at some point and the user has clicked on the button in such a way that the
// animation no longer needs to be displayed again.
constexpr char kShowImageButtonAnimation[] =
    "ash.vc.show_inmage_button_animation";
constexpr char kShowCreateWithAiButtonAnimation[] =
    "ash.vc.show_create_with_ai_button_animation";

// VC nudge ids vector that is iterated whenever `CloseAllVcNudges()` is
// called. Please keep in sync whenever adding/removing/updating a nudge id.
const char* const kNudgeIds[] = {
    kVideoConferenceTraySpeakOnMuteOptInNudgeId,
    kVideoConferenceTraySpeakOnMuteDetectedNudgeId,
    kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId,
    kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId,
    kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId,
    kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId};

constexpr auto kRepeatedShowTimerInterval = base::Milliseconds(100);
constexpr auto kHandleDeviceUsedWhileDisabledWaitTime = base::Milliseconds(200);

// The max amount of times the "Speak-on-mute opt-in" nudge can show.
// As speak-on-mute prefs sync across devices, we need to double check with Sync
// team if this constant grows significantly (e.g. to 50).
constexpr int kSpeakOnMuteOptInNudgeMaxShownCount = 3;

// The max amount of times the "Speak-on-mute" nudge can show in a
// single session.
constexpr int kSpeakOnMuteDetectedNudgeMaxShownCount = 4;

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
  auto* window = Shell::Get()->GetRootWindowForNewWindows();
  if (!window) {
    return nullptr;
  }

  auto* root_window_controller = RootWindowController::ForWindow(window);
  if (!root_window_controller) {
    return nullptr;
  }

  auto* status_area_widget = root_window_controller->GetStatusAreaWidget();
  if (!status_area_widget) {
    return nullptr;
  }

  return status_area_widget->video_conference_tray();
}

PrefService* GetActiveUserPrefService() {
  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(pref_service);
  return pref_service;
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
void VideoConferenceTrayController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kShowImageButtonAnimation, true);
  registry->RegisterBooleanPref(kShowCreateWithAiButtonAnimation, true);
}

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

void VideoConferenceTrayController::CreateNudgeRequest(
    std::unique_ptr<AnchoredNudgeData> nudge_data) {
  // Ignore new requests if we already have one pending nudge.
  if (requested_nudge_data_) {
    return;
  }
  requested_nudge_data_ = std::move(nudge_data);

  auto* active_vc_tray = GetVcTrayInActiveWindow();
  if (!active_vc_tray) {
    return;
  }

  // Attempt showing the nudge immediately if tray is not animating.
  if (!active_vc_tray->layer()->GetAnimator()->is_animating()) {
    MaybeRunNudgeRequest();
  }
}

void VideoConferenceTrayController::MaybeRunNudgeRequest() {
  if (!requested_nudge_data_) {
    return;
  }
  AnchoredNudgeManager::Get()->Show(*requested_nudge_data_);
  requested_nudge_data_.reset();
}

void VideoConferenceTrayController::MaybeShowSpeakOnMuteOptInNudge() {
  auto* active_vc_tray = GetVcTrayInActiveWindow();
  if (!active_vc_tray) {
    return;
  }

  // Only attempt to show the speak-on-mute opt-in nudge if the tray is visible
  // preferred in the active display, and microphone input is muted.
  if (!active_vc_tray->visible_preferred() || !GetMicrophoneMuted()) {
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

  views::View* anchor_view = active_vc_tray->audio_icon();
  if (!anchor_view->GetVisible()) {
    return;
  }

  AnchoredNudgeData nudge_data(
      kVideoConferenceTraySpeakOnMuteOptInNudgeId,
      NudgeCatalogName::kVideoConferenceTraySpeakOnMuteOptIn,
      l10n_util::GetStringUTF16(
          IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_BODY),
      anchor_view);

  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_SPEAK_ON_MUTE_OPT_IN_NUDGE_IMAGE);

  nudge_data.title_text = l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_TITLE);

  nudge_data.primary_button_text = l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_PRIMARY_BUTTON);
  nudge_data.primary_button_callback = base::BindRepeating(
      &VideoConferenceTrayController::OnSpeakOnMuteNudgeOptInAction,
      weak_ptr_factory_.GetWeakPtr(), /*opt_in=*/true);

  nudge_data.secondary_button_text = l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_SECONDARY_BUTTON);
  nudge_data.secondary_button_callback = base::BindRepeating(
      &VideoConferenceTrayController::OnSpeakOnMuteNudgeOptInAction,
      weak_ptr_factory_.GetWeakPtr(), /*opt_in=*/false);

  nudge_data.duration = NudgeDuration::kLongDuration;
  nudge_data.anchored_to_shelf = true;

  AnchoredNudgeManager::Get()->Show(nudge_data);

  pref_service->SetInteger(
      prefs::kSpeakOnMuteOptInNudgeShownCount,
      pref_service->GetInteger(prefs::kSpeakOnMuteOptInNudgeShownCount) + 1);

  if (pref_service->GetInteger(prefs::kSpeakOnMuteOptInNudgeShownCount) >=
      kSpeakOnMuteOptInNudgeMaxShownCount) {
    pref_service->SetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge, false);
  }
}

void VideoConferenceTrayController::DismissImageButtonAnimationForever() {
  GetActiveUserPrefService()->SetBoolean(kShowImageButtonAnimation, false);
}

void VideoConferenceTrayController::
    DismissCreateWithAiButtonAnimationForever() {
  GetActiveUserPrefService()->SetBoolean(kShowCreateWithAiButtonAnimation,
                                         false);
}

bool VideoConferenceTrayController::ShouldShowImageButtonAnimation() const {
  return GetActiveUserPrefService()->GetBoolean(kShowImageButtonAnimation);
}

bool VideoConferenceTrayController::ShouldShowCreateWithAiButtonAnimation()
    const {
  return GetActiveUserPrefService()->GetBoolean(
      kShowCreateWithAiButtonAnimation);
}

void VideoConferenceTrayController::OnSpeakOnMuteNudgeOptInAction(bool opt_in) {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }

  pref_service->SetBoolean(prefs::kShouldShowSpeakOnMuteOptInNudge, false);
  pref_service->SetBoolean(prefs::kUserSpeakOnMuteDetectionEnabled, opt_in);

  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kVideoConferenceTraySpeakOnMuteOptIn);

  // Show the opt-in/out confirmation toast.
  ToastData toast_data(
      kVideoConferenceTraySpeakOnMuteOptInConfirmationToastId,
      ToastCatalogName::kVideoConferenceTraySpeakOnMuteOptInConfirmation,
      l10n_util::GetStringUTF16(
          opt_in
              ? IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_CONFIRMATION_BODY
              : IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_OUT_CONFIRMATION_BODY),
      ToastData::kDefaultToastDuration,
      /*visible_on_lock_screen=*/false,
      /*has_dismiss_button=*/true,
      l10n_util::GetStringUTF16(
          IDS_ASH_VIDEO_CONFERENCE_NUDGE_SPEAK_ON_MUTE_OPT_IN_CONFIRMATION_BUTTON));
  toast_data.persist_on_hover = true;
  toast_data.show_on_all_root_windows = true;
  toast_data.dismiss_callback = base::BindRepeating([]() {
    Shell::Get()
        ->system_tray_model()
        ->client()
        ->ShowSpeakOnMuteDetectionSettings();
  });

  ToastManager::Get()->Show(std::move(toast_data));
}

void VideoConferenceTrayController::OnDlcDownloadStateFetched(
    bool add_warning,
    const std::u16string& feature_tile_title) {
  for (auto& observer : observer_list_) {
    observer.OnDlcDownloadStateChanged(add_warning, feature_tile_title);
  }
}

void VideoConferenceTrayController::CloseAllVcNudges() {
  for (size_t i = 0; i < std::size(kNudgeIds); ++i) {
    AnchoredNudgeManager::Get()->Cancel(kNudgeIds[i]);
  }
}

bool VideoConferenceTrayController::IsAnyVcNudgeShown() {
  for (size_t i = 0; i < std::size(kNudgeIds); ++i) {
    if (Shell::Get()->anchored_nudge_manager()->IsNudgeShown(kNudgeIds[i])) {
      return true;
    }
  }
  return false;
}

bool VideoConferenceTrayController::GetHasCameraPermissions() const {
  return state_.has_camera_permission;
}

bool VideoConferenceTrayController::GetHasMicrophonePermissions() const {
  return state_.has_microphone_permission;
}

void VideoConferenceTrayController::UpdateSidetoneSupportedState() {
  CrasAudioHandler::Get()->UpdateSidetoneSupportedState();
}

bool VideoConferenceTrayController::IsSidetoneSupported() const {
  return CrasAudioHandler::Get()->IsSidetoneSupported();
}

bool VideoConferenceTrayController::GetSidetoneEnabled() const {
  return CrasAudioHandler::Get()->GetSidetoneEnabled();
}

void VideoConferenceTrayController::SetSidetoneEnabled(bool enabled) {
  CrasAudioHandler::Get()->SetSidetoneEnabled(enabled);
}

void VideoConferenceTrayController::SetEwmaPowerReportEnabled(bool enabled) {
  CrasAudioHandler::Get()->SetEwmaPowerReportEnabled(enabled);
}

double VideoConferenceTrayController::GetEwmaPower() {
  return CrasAudioHandler::Get()->GetEwmaPower();
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

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref_service && !pref_service->GetBoolean(prefs::kUserCameraAllowed);
}

void VideoConferenceTrayController::SetMicrophoneMuted(bool muted) {
  // Change user pref to let Privacy Hub enable/disable the microphone.
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }
  pref_service->SetBoolean(prefs::kUserMicrophoneAllowed, !muted);
}

bool VideoConferenceTrayController::GetMicrophoneMuted() {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref_service &&
         !pref_service->GetBoolean(prefs::kUserMicrophoneAllowed);
}

void VideoConferenceTrayController::StopAllScreenShare() {
  CHECK(video_conference_manager_);
  video_conference_manager_->StopAllScreenShare();
}

void VideoConferenceTrayController::GetMediaApps(
    base::OnceCallback<void(MediaApps)> ui_callback) {
  CHECK(video_conference_manager_);
  video_conference_manager_->GetMediaApps(std::move(ui_callback));
}

void VideoConferenceTrayController::ReturnToApp(
    const base::UnguessableToken& id) {
  CHECK(video_conference_manager_);
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
    auto* nudge_manager = AnchoredNudgeManager::Get();

    nudge_manager->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraUseWhileHWDisabled);
    nudge_manager->Cancel(kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId);

    nudge_manager->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraMicrophoneUseWhileDisabled);
    nudge_manager->Cancel(kVideoConferenceTrayBothUseWhileDisabledNudgeId);
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
    auto* nudge_manager = AnchoredNudgeManager::Get();

    nudge_manager->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraUseWhileSWDisabled);
    nudge_manager->Cancel(kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId);

    nudge_manager->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraMicrophoneUseWhileDisabled);
    nudge_manager->Cancel(kVideoConferenceTrayBothUseWhileDisabledNudgeId);
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

  if (mute_on) {
    // Resets the speak-on-mute nudge status so that notification can pop-up
    // when mic changes to muted.
    last_speak_on_mute_nudge_shown_time_ = base::TimeTicks();
    speak_on_mute_nudge_shown_count_ = 0;

    // Attempt showing the speak-on-mute opt-in nudge when input is muted.
    MaybeShowSpeakOnMuteOptInNudge();
  } else {
    auto* nudge_manager = AnchoredNudgeManager::Get();

    // Cancel speak-on-mute opt-in nudge if one was being shown.
    nudge_manager->Cancel(kVideoConferenceTraySpeakOnMuteOptInNudgeId);

    // Attempt recording "Speak-on-mute" nudge action when mic is unmuted.
    nudge_manager->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTraySpeakOnMuteDetected);
    nudge_manager->Cancel(kVideoConferenceTraySpeakOnMuteDetectedNudgeId);

    // Attempt recording "Use while disabled" nudge action when mic is unmuted.
    nudge_manager->MaybeRecordNudgeAction(
        microphone_muted_by_hardware_switch_
            ? NudgeCatalogName::kVideoConferenceTrayMicrophoneUseWhileHWDisabled
            : NudgeCatalogName::
                  kVideoConferenceTrayMicrophoneUseWhileSWDisabled);
    nudge_manager->Cancel(
        microphone_muted_by_hardware_switch_
            ? kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId
            : kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId);

    nudge_manager->MaybeRecordNudgeAction(
        NudgeCatalogName::kVideoConferenceTrayCameraMicrophoneUseWhileDisabled);
    nudge_manager->Cancel(kVideoConferenceTrayBothUseWhileDisabledNudgeId);
  }
}

void VideoConferenceTrayController::OnSpeakOnMuteDetected() {
  // Do not show "Speak on mute" nudge if another nudge is showing.
  if (IsAnyVcNudgeShown()) {
    return;
  }

  auto* active_vc_tray = GetVcTrayInActiveWindow();
  if (!active_vc_tray) {
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();

  // Only shows "Speak on mute" nudge if one of the following conditions meets:
  // 1. The nudge has never shown in the current session.
  // 2. The nudge has not shown for maximum times in the current session and the
  // cool down has passed.
  if (speak_on_mute_nudge_shown_count_ == 0 ||
      (speak_on_mute_nudge_shown_count_ <
           kSpeakOnMuteDetectedNudgeMaxShownCount &&
       (current_time - last_speak_on_mute_nudge_shown_time_).InSeconds() >=
           60 * std::pow(2, speak_on_mute_nudge_shown_count_))) {
    AnchoredNudgeData nudge_data(
        kVideoConferenceTraySpeakOnMuteDetectedNudgeId,
        NudgeCatalogName::kVideoConferenceTraySpeakOnMuteDetected,
        l10n_util::GetStringUTF16(
            IDS_ASH_VIDEO_CONFERENCE_TOAST_SPEAK_ON_MUTE_DETECTED),
        /*anchor_view=*/active_vc_tray->audio_icon());
    // Opens the privacy hub settings page with the mute nudge focused when
    // clicking on the nudge.
    nudge_data.click_callback = base::BindRepeating([]() -> void {
      Shell::Get()
          ->system_tray_model()
          ->client()
          ->ShowSpeakOnMuteDetectionSettings();
    });
    nudge_data.anchored_to_shelf = true;
    AnchoredNudgeManager::Get()->Show(nudge_data);

    // Updates the counter and the nudge last shown time.
    last_speak_on_mute_nudge_shown_time_ = current_time;
    ++speak_on_mute_nudge_shown_count_;
  }
}

void VideoConferenceTrayController::OnUserSessionAdded(
    const AccountId& account_id) {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }
}

void VideoConferenceTrayController::OnShellDestroying() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
}

void VideoConferenceTrayController::HandleClientUpdate(
    crosapi::mojom::VideoConferenceClientUpdatePtr update) {
  // Use `HandleClientUpdate()` to detect apps being
  // added because this function is guaranteed to be called when an app is
  // added, even if the `VideoConferecenMediaState` does not change.

  if (update->added_or_removed_app ==
      crosapi::mojom::VideoConferenceAppUpdate::kAppAdded) {
    OnAppAdded();
  }
}

void VideoConferenceTrayController::OnAppAdded() {
  // If any `Shelf` is auto hidden, the `Shelf` needs to be forced shown, or
  // allowed to hide, depending on the current number of capturing applications.
  if (!IsAnyShelfAutoHidden()) {
    return;
  }

  // If a new app has begun capturing, lock all shelfs to force them to show.
  // This lock will apply until whichever comes first:
  // `disable_shelf_autohide_timer_` fires, or the number of capturing apps
  // drops to 0 (handled at `UpdateWithMediaState()`).
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

base::OneShotTimer&
VideoConferenceTrayController::GetShelfAutoHideTimerForTest() {
  return disable_shelf_autohide_timer_;
}

VideoConferenceTrayEffectsManager&
VideoConferenceTrayController::GetEffectsManager() {
  return effects_manager_;
}

void VideoConferenceTrayController::CreateBackgroundImage() {
  CHECK(video_conference_manager_);
  video_conference_manager_->CreateBackgroundImage();
}

void VideoConferenceTrayController::UpdateWithMediaState(
    VideoConferenceMediaState state) {
  auto old_state = state_;
  const bool old_tray_target_visibility = ShouldShowTray();
  state_ = state;
  const bool new_tray_target_visibility = ShouldShowTray();

  if (new_tray_target_visibility && !old_tray_target_visibility) {
    GetEffectsManager().RecordInitialStates();

    // Keeps increment the count to track the number of times the view flickers.
    // When the delay of `kRepeatedShowTimerInterval` has reached, record that
    // count.
    ++count_repeated_shows_;
    repeated_shows_timer_.Reset();

    // Resets the speak-on-mute nudge status so that notification can pop-up
    // when new VC tray appears.
    last_speak_on_mute_nudge_shown_time_ = base::TimeTicks();
    speak_on_mute_nudge_shown_count_ = 0;
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

  if (state_.has_media_app) {
    return;
  }

  // If no more apps are capturing, release all shelf autohide locks. These
  // locks are created when a new app begins capturing at
  // `VideoConferenceTrayController::HandleClientUpdate()`.
  if (disable_shelf_autohide_timer_.IsRunning()) {
    disable_shelf_autohide_timer_.Stop();
  }
  disable_shelf_autohide_locks_.clear();
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
  if (device == crosapi::mojom::VideoConferenceMediaDevice::kUnusedDefault) {
    return;
  }

  UsedWhileDisabledNudgeType type = GetUsedWhileDisabledNudgeType(device);

  if (!use_while_disabled_signal_waiter_.IsRunning()) {
    // Cache the type and starts the timer to wait for the signal of the other
    // device.
    use_while_disabled_nudge_on_wait_ = type;

    use_while_disabled_signal_waiter_.Start(
        FROM_HERE, kHandleDeviceUsedWhileDisabledWaitTime,
        base::BindOnce(
            &VideoConferenceTrayController::DisplayUsedWhileDisabledNudge,
            weak_ptr_factory_.GetWeakPtr(), type, app_name));
    return;
  }

  if (type == use_while_disabled_nudge_on_wait_) {
    return;
  }

  use_while_disabled_signal_waiter_.Stop();

  // If we receive the signal for both camera and microphone, display the nudge
  // for both.
  DisplayUsedWhileDisabledNudge(UsedWhileDisabledNudgeType::kBoth, app_name);
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

void VideoConferenceTrayController::DisplayUsedWhileDisabledNudge(
    VideoConferenceTrayController::UsedWhileDisabledNudgeType type,
    const std::u16string& app_name) {
  // Do not show "Use while disabled" nudge if another nudge is showing.
  if (IsAnyVcNudgeShown()) {
    return;
  }

  auto* active_vc_tray = GetVcTrayInActiveWindow();
  if (!active_vc_tray) {
    return;
  }

  std::u16string device_name;
  int text_id;
  NudgeCatalogName catalog_name;
  std::string nudge_id;
  views::View* anchor_view = nullptr;
  switch (type) {
    case VideoConferenceTrayController::UsedWhileDisabledNudgeType::kMicrophone:
      device_name =
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_MICROPHONE_NAME);
      if (microphone_muted_by_hardware_switch_) {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED;
        nudge_id = kVideoConferenceTrayMicrophoneUseWhileHWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayMicrophoneUseWhileHWDisabled;
      } else {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_DISABLED;
        nudge_id = kVideoConferenceTrayMicrophoneUseWhileSWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayMicrophoneUseWhileSWDisabled;
      }
      anchor_view = active_vc_tray->audio_icon();
      break;
    case VideoConferenceTrayController::UsedWhileDisabledNudgeType::kCamera:
      device_name =
          l10n_util::GetStringUTF16(IDS_ASH_VIDEO_CONFERENCE_CAMERA_NAME);
      if (camera_muted_by_hardware_switch_) {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_HARDWARE_DISABLED;
        nudge_id = kVideoConferenceTrayCameraUseWhileHWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayCameraUseWhileHWDisabled;
      } else {
        text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_DISABLED;
        nudge_id = kVideoConferenceTrayCameraUseWhileSWDisabledNudgeId;
        catalog_name =
            NudgeCatalogName::kVideoConferenceTrayCameraUseWhileSWDisabled;
      }
      anchor_view = active_vc_tray->camera_icon();
      break;
    case VideoConferenceTrayController::UsedWhileDisabledNudgeType::kBoth:
      device_name = l10n_util::GetStringUTF16(
          IDS_ASH_VIDEO_CONFERENCE_CAMERA_MICROPHONE_NAME);
      text_id = IDS_ASH_VIDEO_CONFERENCE_TOAST_USE_WHILE_DISABLED;
      nudge_id = kVideoConferenceTrayBothUseWhileDisabledNudgeId;
      catalog_name = NudgeCatalogName::
          kVideoConferenceTrayCameraMicrophoneUseWhileDisabled;
      anchor_view = active_vc_tray->audio_icon();
      break;
    default:
      NOTREACHED();
  }

  AnchoredNudgeData nudge_data(
      nudge_id, catalog_name,
      l10n_util::GetStringFUTF16(text_id, app_name, device_name), anchor_view);
  nudge_data.anchored_to_shelf = true;
  CreateNudgeRequest(
      std::make_unique<AnchoredNudgeData>(std::move(nudge_data)));
}

VideoConferenceTrayController::UsedWhileDisabledNudgeType
VideoConferenceTrayController::GetUsedWhileDisabledNudgeType(
    crosapi::mojom::VideoConferenceMediaDevice device) {
  DCHECK_NE(device, crosapi::mojom::VideoConferenceMediaDevice::kUnusedDefault);

  VideoConferenceTrayController::UsedWhileDisabledNudgeType type;
  switch (device) {
    case crosapi::mojom::VideoConferenceMediaDevice::kCamera:
      type = VideoConferenceTrayController::UsedWhileDisabledNudgeType::kCamera;
      break;
    case crosapi::mojom::VideoConferenceMediaDevice::kMicrophone:
      type = VideoConferenceTrayController::UsedWhileDisabledNudgeType::
          kMicrophone;
      break;
    default:
      NOTREACHED();
  }

  return type;
}

}  // namespace ash
