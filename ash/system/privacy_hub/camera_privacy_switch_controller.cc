// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/system_notification_controller.h"
#include "base/check.h"
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

}  // namespace

CameraPrivacySwitchController::CameraPrivacySwitchController()
    : switch_api_(std::make_unique<VCDPrivacyAdapter>()),
      turn_sw_switch_on_notification_(
          kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId,
          NotificationCatalogName::kPrivacyHubHWCameraSwitchOffSWCameraSwitchOn,
          PrivacyHubNotificationDescriptor{
              SensorDisabledNotificationDelegate::SensorSet{},
              IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_TITLE,
              std::vector<int>{IDS_PRIVACY_HUB_TURN_OFF_CAMERA_ACTION_BUTTON},
              std::vector<int>{
                  IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_MESSAGE},
              base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
                  base::BindRepeating([]() {
                    PrivacyHubNotificationController::
                        SetAndLogSensorPreferenceFromNotification(
                            SensorDisabledNotificationDelegate::Sensor::kCamera,
                            false);
                  }))}) {
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

  // Add camera observers after `pref_change_registrar_` is created because
  // `OnCameraSWPrivacySwitchStateChanged` accesses a pref value.
  if (!is_camera_observer_added_) {
    // Subscribe to the camera HW/SW privacy switch events.
    auto device_id_to_privacy_switch_state =
        media::CameraHalDispatcherImpl::GetInstance()
            ->AddCameraPrivacySwitchObserver(this);
    is_camera_observer_added_ = true;
  }

  // To ensure consistent values between the user pref and camera backend
  OnPreferenceChanged(prefs::kUserCameraAllowed);
}

void CameraPrivacySwitchController::OnPreferenceChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kUserCameraAllowed);
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  switch_api_->SetCameraSWPrivacySwitch(pref_val);

  turn_sw_switch_on_notification_.Hide();

  // Always remove the sensor disabled notification if the sensor was unmuted.
  if (pref_val == CameraSWPrivacySwitchSetting::kEnabled) {
    PrivacyHubNotificationController::Get()->RemoveSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kCamera);
  }
}

void CameraPrivacySwitchController::OnCameraCountChanged(int new_camera_count) {
  camera_count_ = new_camera_count;
}

CameraSWPrivacySwitchSetting
CameraPrivacySwitchController::GetUserSwitchPreference() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());
  const bool allowed =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kUserCameraAllowed);

  return allowed ? CameraSWPrivacySwitchSetting::kEnabled
                 : CameraSWPrivacySwitchSetting::kDisabled;
}

void CameraPrivacySwitchController::SetCameraPrivacySwitchAPIForTest(
    std::unique_ptr<CameraPrivacySwitchAPI> switch_api) {
  DCHECK(switch_api);
  switch_api_ = std::move(switch_api);
}

void CameraPrivacySwitchController::OnCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
    cros::mojom::CameraPrivacySwitchState state) {
  if (features::IsVideoConferenceEnabled() ||
      features::IsPrivacyIndicatorsEnabled()) {
    return;
  }

  // Issue a notification if camera is disabled by HW switch, but not by the SW
  // switch and there are multiple cameras.
  if (state == cros::mojom::CameraPrivacySwitchState::ON &&
      GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kEnabled &&
      camera_count_ > 1) {
    turn_sw_switch_on_notification_.Show();
  }
  if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
    // Clear the notification that might have been displayed earlier
    turn_sw_switch_on_notification_.Hide();
  }
}

void CameraPrivacySwitchController::OnCameraSWPrivacySwitchStateChanged(
    cros::mojom::CameraPrivacySwitchState state) {
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  cros::mojom::CameraPrivacySwitchState pref_state =
      pref_val == CameraSWPrivacySwitchSetting::kEnabled
          ? cros::mojom::CameraPrivacySwitchState::OFF
          : cros::mojom::CameraPrivacySwitchState::ON;
  if (state != pref_state) {
    switch_api_->SetCameraSWPrivacySwitch(pref_val);
  }
}

void CameraPrivacySwitchController::ActiveApplicationsChanged(
    bool application_added) {
  if (application_added) {
    active_applications_using_camera_count_++;
  } else {
    DCHECK_GT(active_applications_using_camera_count_, 0);
    active_applications_using_camera_count_--;
  }

  const bool camera_muted_by_sw =
      GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kDisabled;

  if (features::IsVideoConferenceEnabled()) {
    // The `VideoConferenceTrayController` shows this info as a toast.
    return;
  }

  auto* privacy_hub_notification_controller =
      PrivacyHubNotificationController::Get();
  CHECK(privacy_hub_notification_controller);

  if (features::IsPrivacyIndicatorsEnabled()) {
    // NOTE: This logic mirrors the logic in
    // `MicrophonePrivacySwitchController`.
    if (active_applications_using_camera_count_ == 0) {
      // Always remove the notification when active applications go to 0.
      privacy_hub_notification_controller->RemoveSoftwareSwitchNotification(
          SensorDisabledNotificationDelegate::Sensor::kCamera);
    } else if (application_added) {
      if (camera_muted_by_sw) {
        privacy_hub_notification_controller->ShowSoftwareSwitchNotification(
            SensorDisabledNotificationDelegate::Sensor::kCamera);
      }
    } else {
      // Application removed, update the notifications message.
      privacy_hub_notification_controller->UpdateSoftwareSwitchNotification(
          SensorDisabledNotificationDelegate::Sensor::kCamera);
    }
    return;
  }

  if (!camera_muted_by_sw) {
    return;
  }

  if (active_applications_using_camera_count_ == 0 &&
      camera_used_while_deactivated_) {
    camera_used_while_deactivated_ = false;
    privacy_hub_notification_controller->RemoveSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kCamera);
  } else if (application_added) {
    camera_used_while_deactivated_ = true;
    privacy_hub_notification_controller->ShowSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kCamera);
  } else {
    privacy_hub_notification_controller->UpdateSoftwareSwitchNotification(
        SensorDisabledNotificationDelegate::Sensor::kCamera);
  }
}

}  // namespace ash
