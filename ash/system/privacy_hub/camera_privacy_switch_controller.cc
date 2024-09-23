// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <cstddef>
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
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
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

CameraPrivacySwitchController::CameraPrivacySwitchController()
    : switch_api_(std::make_unique<VCDPrivacyAdapter>()) {
  Shell::Get()->session_controller()->AddObserver(this);
}

CameraPrivacySwitchController::~CameraPrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  media::CameraHalDispatcherImpl::GetInstance()
      ->RemoveCameraPrivacySwitchObserver(this);
}

void CameraPrivacySwitchController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Subscribing again to pref changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kUserCameraAllowed,
      base::BindRepeating(&CameraPrivacySwitchController::OnPreferenceChanged,
                          base::Unretained(this)));

  if (!is_camera_observer_added_) {
    // Subscribe to the camera HW/SW privacy switch events.
    auto device_id_to_privacy_switch_state =
        media::CameraHalDispatcherImpl::GetInstance()
            ->AddCameraPrivacySwitchObserver(this);
    is_camera_observer_added_ = true;
  }

  if (force_disable_camera_access_) {
    StorePreviousPrefValue();
    prefs().SetBoolean(prefs::kUserCameraAllowed, false);
  } else {
    // It's possible we crashed while force disable camera access was enabled,
    // in which case we need to restore the previous pref value.
    RestorePreviousPrefValueMaybe();
  }

  // To ensure consistent values between the user pref and camera backend.
  OnPreferenceChanged(prefs::kUserCameraAllowed);
}

void CameraPrivacySwitchController::OnCameraSWPrivacySwitchStateChanged(
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

void CameraPrivacySwitchController::OnPreferenceChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kUserCameraAllowed);

  // Always remove the sensor disabled notification if the sensor was unmuted.
  if (GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kEnabled) {
    PrivacyHubNotificationController::Get()->RemoveSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kCamera);
  }

  if (force_disable_camera_access_ &&
      GetUserSwitchPreference() != CameraSWPrivacySwitchSetting::kDisabled) {
    prefs().SetBoolean(prefs::kUserCameraAllowed, false);
  }

  // This needs to be called after RemoveSoftwareSwitchNotification() as that
  // call can change the pref value.
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  switch_api_->SetCameraSWPrivacySwitch(pref_val);
}

CameraSWPrivacySwitchSetting
CameraPrivacySwitchController::GetUserSwitchPreference() const {
  const bool allowed = prefs().GetBoolean(prefs::kUserCameraAllowed);

  return allowed ? CameraSWPrivacySwitchSetting::kEnabled
                 : CameraSWPrivacySwitchSetting::kDisabled;
}

void CameraPrivacySwitchController::SetCameraPrivacySwitchAPIForTest(
    std::unique_ptr<CameraPrivacySwitchAPI> switch_api) {
  DCHECK(switch_api);
  switch_api_ = std::move(switch_api);
}

void CameraPrivacySwitchController::SetCameraSWPrivacySwitch(
    CameraSWPrivacySwitchSetting value) {
  switch_api_->SetCameraSWPrivacySwitch(value);
}

void CameraPrivacySwitchController::SetUserSwitchPreference(
    CameraSWPrivacySwitchSetting value) {
  prefs().SetBoolean(prefs::kUserCameraAllowed,
                     value == CameraSWPrivacySwitchSetting::kEnabled);
}

void CameraPrivacySwitchController::SetFrontend(PrivacyHubDelegate* frontend) {
  frontend_ = frontend;
}

void CameraPrivacySwitchController::SetForceDisableCameraAccess(
    bool new_value) {
  force_disable_camera_access_ = new_value;
  if (pref_change_registrar_) {
    if (new_value) {
      StorePreviousPrefValue();
      prefs().SetBoolean(prefs::kUserCameraAllowed, false);
    } else {
      RestorePreviousPrefValueMaybe();
    }
  }

  if (frontend_) {
    frontend_->SetForceDisableCameraSwitch(new_value);
  }
}

bool CameraPrivacySwitchController::IsCameraAccessForceDisabled() const {
  return force_disable_camera_access_;
}

void CameraPrivacySwitchController::StorePreviousPrefValue() {
  if (prefs().HasPrefPath(prefs::kUserCameraAllowedPreviousValue)) {
    // Do not overwrite previous stored value, otherwise force disabling
    // camera access twice in a row will not properly restore the previous
    // value.
    return;
  }

  prefs().SetBoolean(prefs::kUserCameraAllowedPreviousValue,
                     prefs().GetBoolean(prefs::kUserCameraAllowed));
}

void CameraPrivacySwitchController::RestorePreviousPrefValueMaybe() {
  // If a previous value was stored, restore it and then clear the stored
  // previous value so we do not keep restoring it.
  if (prefs().HasPrefPath(prefs::kUserCameraAllowedPreviousValue)) {
    prefs().SetBoolean(
        prefs::kUserCameraAllowed,
        prefs().GetBoolean(prefs::kUserCameraAllowedPreviousValue));

    prefs().ClearPref(prefs::kUserCameraAllowedPreviousValue);
  }
}

PrefService& CameraPrivacySwitchController::prefs() {
  CHECK(pref_change_registrar_);
  return CHECK_DEREF(pref_change_registrar_->prefs());
}

const PrefService& CameraPrivacySwitchController::prefs() const {
  CHECK(pref_change_registrar_);
  return CHECK_DEREF(pref_change_registrar_->prefs());
}

// static
CameraPrivacySwitchController* CameraPrivacySwitchController::Get() {
  PrivacyHubController* privacy_hub_controller =
      Shell::Get()->privacy_hub_controller();
  return privacy_hub_controller ? privacy_hub_controller->camera_controller()
                                : nullptr;
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
  auto* privacy_hub_controller = PrivacyHubController::Get();
  CHECK(privacy_hub_controller);
  return privacy_hub_controller->UsingCameraLEDFallback();
}

bool CameraPrivacySwitchController::IsCameraUsageAllowed() const {
  switch (GetUserSwitchPreference()) {
    case CameraSWPrivacySwitchSetting::kEnabled:
      return true;
    case CameraSWPrivacySwitchSetting::kDisabled:
      return false;
  }
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

}  // namespace ash
