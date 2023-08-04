// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/system/system_notification_controller.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "camera_privacy_switch_controller.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"

namespace ash {

namespace {

// Wraps and adapts the VCD API.
// It is used for dependency injection, so that we can write
// mock tests for CameraController easily.
class VCDPrivacyAdapter : public CameraPrivacySwitchAPI {
 public:
  // CameraPrivacySwitchAPI:
  void SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting) override;
};

void VCDPrivacyAdapter::SetCameraSWPrivacySwitch(
    CameraSWPrivacySwitchSetting camera_switch_setting) {
  switch (camera_switch_setting) {
    case CameraSWPrivacySwitchSetting::kEnabled: {
      media::CameraHalDispatcherImpl::GetInstance()
          ->SetCameraSWPrivacySwitchState(
              cros::mojom::CameraPrivacySwitchState::OFF);
      break;
    }
    case CameraSWPrivacySwitchSetting::kDisabled: {
      media::CameraHalDispatcherImpl::GetInstance()
          ->SetCameraSWPrivacySwitchState(
              cros::mojom::CameraPrivacySwitchState::ON);
      break;
    }
  }
}

const base::TimeDelta kCameraLedFallbackNotificationExtensionPeriod =
    base::Seconds(30);

}  // namespace

CameraPrivacySwitchSynchronizer::CameraPrivacySwitchSynchronizer()
    : switch_api_(std::make_unique<VCDPrivacyAdapter>()) {
  Shell::Get()->session_controller()->AddObserver(this);
}

CameraPrivacySwitchSynchronizer::~CameraPrivacySwitchSynchronizer() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  media::CameraHalDispatcherImpl::GetInstance()
      ->RemoveCameraPrivacySwitchObserver(this);
}

void CameraPrivacySwitchSynchronizer::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Subscribing again to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserCameraAllowed,
      base::BindRepeating(&CameraPrivacySwitchSynchronizer::OnPreferenceChanged,
                          base::Unretained(this)));

  if (!is_camera_observer_added_) {
    // Subscribe to the camera HW/SW privacy switch events.
    auto device_id_to_privacy_switch_state =
        media::CameraHalDispatcherImpl::GetInstance()
            ->AddCameraPrivacySwitchObserver(this);
    is_camera_observer_added_ = true;
  }

  // To ensure consistent values between the user pref and camera backend.
  OnPreferenceChanged(prefs::kUserCameraAllowed);
}

void CameraPrivacySwitchSynchronizer::OnCameraSWPrivacySwitchStateChanged(
    // This makes sure that the backend state is in sync with the pref.
    // The backend service sometimes may have a wrong camera switch state after
    // restart. This is necessary to correct it.
    cros::mojom::CameraPrivacySwitchState state) {
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  // Note that camera ON means privacy switch OFF.
  cros::mojom::CameraPrivacySwitchState pref_state =
      pref_val == CameraSWPrivacySwitchSetting::kEnabled
          ? cros::mojom::CameraPrivacySwitchState::OFF
          : cros::mojom::CameraPrivacySwitchState::ON;
  if (state != pref_state) {
    SetCameraSWPrivacySwitch(pref_val);
  }
}

void CameraPrivacySwitchSynchronizer::OnPreferenceChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kUserCameraAllowed);

  OnPreferenceChangedImpl();

  // This needs to be called after OnPreferenceChangedImpl() as that call can
  // change the pref value.
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  switch_api_->SetCameraSWPrivacySwitch(pref_val);
}

CameraSWPrivacySwitchSetting
CameraPrivacySwitchSynchronizer::GetUserSwitchPreference() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());
  const bool allowed =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kUserCameraAllowed);

  return allowed ? CameraSWPrivacySwitchSetting::kEnabled
                 : CameraSWPrivacySwitchSetting::kDisabled;
}

void CameraPrivacySwitchSynchronizer::SetCameraPrivacySwitchAPIForTest(
    std::unique_ptr<CameraPrivacySwitchAPI> switch_api) {
  DCHECK(switch_api);
  switch_api_ = std::move(switch_api);
}

void CameraPrivacySwitchSynchronizer::SetCameraSWPrivacySwitch(
    CameraSWPrivacySwitchSetting value) {
  switch_api_->SetCameraSWPrivacySwitch(value);
}

void CameraPrivacySwitchSynchronizer::SetUserSwitchPreference(
    CameraSWPrivacySwitchSetting value) {
  CHECK(pref_change_registrar_);
  CHECK(pref_change_registrar_->prefs());
  pref_change_registrar_->prefs()->SetBoolean(
      prefs::kUserCameraAllowed,
      value == CameraSWPrivacySwitchSetting::kEnabled);
}

CameraPrivacySwitchController::CameraPrivacySwitchController() {
  InitUsingCameraLEDFallback();
}

CameraPrivacySwitchController::~CameraPrivacySwitchController() = default;

// static
CameraPrivacySwitchController* CameraPrivacySwitchController::Get() {
  PrivacyHubController* privacy_hub_controller =
      Shell::Get()->privacy_hub_controller();
  return privacy_hub_controller ? privacy_hub_controller->camera_controller()
                                : nullptr;
}

void CameraPrivacySwitchController::OnPreferenceChangedImpl() {
  // Always remove the sensor disabled notification if the sensor was unmuted.
  if (GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kEnabled) {
    PrivacyHubNotificationController::Get()->RemoveSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kCamera);
  }
}

void CameraPrivacySwitchController::OnCameraCountChanged(int new_camera_count) {
  camera_count_ = new_camera_count;
}

void CameraPrivacySwitchController::ActiveApplicationsChanged(
    bool application_added) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (application_added) {
    active_applications_using_camera_count_++;
  } else {
    DCHECK_GT(active_applications_using_camera_count_, 0);
    active_applications_using_camera_count_--;
  }

  const bool camera_muted_by_sw =
      GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kDisabled;

  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  if (!camera_muted_by_sw) {
    return;
  }

  if (features::IsVideoConferenceEnabled()) {
    // The `VideoConferenceTrayController` shows this info as a toast.
    return;
  }

  // NOTE: This logic mirrors the logic in
  // `MicrophonePrivacySwitchController`.
  if (active_applications_using_camera_count_ == 0) {
    // Always remove the notification when active applications go to 0.
    RemoveNotification();
  } else if (application_added) {
    if (InNotificationExtensionPeriod()) {
      // Notification is not updated. The extension period is prolonged.
      last_active_notification_update_time_ = base::Time::Now();
    } else {
      ShowNotification();
    }
    if (UsingCameraLEDFallback()) {
      ScheduleNotificationRemoval();
    }
  } else {
    // Application removed, update the notifications message.
    UpdateNotification();
    if (UsingCameraLEDFallback()) {
      ScheduleNotificationRemoval();
    }
  }
}

bool CameraPrivacySwitchController::UsingCameraLEDFallback() {
  return using_camera_led_fallback_;
}

void CameraPrivacySwitchController::InitUsingCameraLEDFallback() {
  using_camera_led_fallback_ = CheckCameraLEDFallbackDirectly();
}

// static
bool CameraPrivacySwitchController::CheckCameraLEDFallbackDirectly() {
  // Check that the file created by the camera service exists.
  const base::FilePath kPath(
      "/run/camera/camera_ids_with_sw_privacy_switch_fallback");
  if (!base::PathExists(kPath) || !base::PathIsReadable(kPath)) {
    // The camera service should create the file always. However we keep this
    // for backward compatibility when deployed with an older version of the OS
    // and forward compatibility when the fallback is eventually dropped.
    return false;
  }
  int64_t file_size{};
  const bool file_size_read_success = base::GetFileSize(kPath, &file_size);
  CHECK(file_size_read_success);

  return (file_size != 0ll);
}

void CameraPrivacySwitchController::ShowNotification() {
  last_active_notification_update_time_ = base::Time::Now();
  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  privacy_hub_notification_controller->ShowSoftwareSwitchNotification(
      SensorDisabledNotificationDelegate::Sensor::kCamera);
}

void CameraPrivacySwitchController::RemoveNotification() {
  if (InNotificationExtensionPeriod()) {
    // Do not remove notification within the extension period.
    return;
  }

  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  last_active_notification_update_time_ = base::Time::Min();
  privacy_hub_notification_controller->RemoveSoftwareSwitchNotification(
      SensorDisabledNotificationDelegate::Sensor::kCamera);
}

void CameraPrivacySwitchController::UpdateNotification() {
  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  last_active_notification_update_time_ = base::Time::Now();
  privacy_hub_notification_controller->UpdateSoftwareSwitchNotification(
      SensorDisabledNotificationDelegate::Sensor::kCamera);
}

void CameraPrivacySwitchController::ScheduleNotificationRemoval() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CameraPrivacySwitchController::RemoveNotification,
                     weak_ptr_factory_.GetWeakPtr()),
      kCameraLedFallbackNotificationExtensionPeriod);
}

bool CameraPrivacySwitchController::InNotificationExtensionPeriod() {
  if (!UsingCameraLEDFallback()) {
    return false;
  }
  return base::Time::Now() < (last_active_notification_update_time_ +
                              kCameraLedFallbackNotificationExtensionPeriod);
}

void CameraPrivacySwitchDisabled::OnPreferenceChangedImpl() {
  DCHECK(!features::IsCrosPrivacyHubEnabled() &&
         !base::FeatureList::IsEnabled(features::kVideoConference));
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  // Only Privacy hub and VC manipulates the pref, therefore if the camera is
  // disabled and the privacy hub and VC is disabled we need to fix the value.
  // This will automatically update the camera backend.
  if (pref_val != CameraSWPrivacySwitchSetting::kEnabled) {
    LOG(WARNING) << "Global camera switch disabled. Re-enabling.";
    SetUserSwitchPreference(CameraSWPrivacySwitchSetting::kEnabled);
  }
}

}  // namespace ash
