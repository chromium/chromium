// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/system_notification_controller.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

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

PrivacyHubNotificationController* GetPrivacyHubNotificationController() {
  return Shell::Get()->system_notification_controller()->privacy_hub();
}

}  // namespace

CameraPrivacySwitchController::CameraPrivacySwitchController()
    : switch_api_(std::make_unique<VCDPrivacyAdapter>()),
      turn_sw_switch_on_notification_(
          kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId,
          IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_TITLE,
          {IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_MESSAGE},
          PrivacyHubNotification::SensorSet(),
          base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
              base::BindRepeating([]() {
                CameraPrivacySwitchController::
                    SetAndLogCameraPreferenceFromNotification(false);
              })),
          ash::NotificationCatalogName::
              kPrivacyHubHWCameraSwitchOffSWCameraSwitchOn,
          IDS_PRIVACY_HUB_TURN_OFF_CAMERA_ACTION_BUTTON) {
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

  // Make sure to add camera observers after pref_change_registrar_ is created
  // because OnCameraSWPrivacySwitchStateChanged accesses a pref value.
  if (!is_camera_observer_added_) {
    // Subscribe to the camera HW/SW privacy switch events.
    auto device_id_to_privacy_switch_state =
        media::CameraHalDispatcherImpl::GetInstance()
            ->AddCameraPrivacySwitchObserver(this);
    // TODO(b/255248909): Handle multiple cameras with privacy controls
    // properly.
    for (const auto& it : device_id_to_privacy_switch_state) {
      cros::mojom::CameraPrivacySwitchState state = it.second;
      if (state == cros::mojom::CameraPrivacySwitchState::ON) {
        camera_privacy_switch_state_ = state;
        break;
      } else if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
        camera_privacy_switch_state_ = state;
      }
    }
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

  if (active_applications_using_camera_count_ == 0)
    return;

  if (pref_val == CameraSWPrivacySwitchSetting::kDisabled) {
    camera_used_while_deactivated_ = true;
    GetPrivacyHubNotificationController()->ShowSensorDisabledNotification(
        PrivacyHubNotificationController::Sensor::kCamera);
  } else {
    camera_used_while_deactivated_ = false;
    GetPrivacyHubNotificationController()->RemoveSensorDisabledNotification(
        PrivacyHubNotificationController::Sensor::kCamera);
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

// static
void CameraPrivacySwitchController::SetAndLogCameraPreferenceFromNotification(
    const bool enabled) {
  PrefService* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (pref_service) {
    pref_service->SetBoolean(prefs::kUserCameraAllowed, enabled);
    privacy_hub_metrics::LogCameraEnabledFromNotification(enabled);
  }
}

void CameraPrivacySwitchController::SetCameraPrivacySwitchAPIForTest(
    std::unique_ptr<CameraPrivacySwitchAPI> switch_api) {
  DCHECK(switch_api);
  switch_api_ = std::move(switch_api);
}

void CameraPrivacySwitchController::OnCameraHWPrivacySwitchStateChanged(
    const std::string& device_id,
    cros::mojom::CameraPrivacySwitchState state) {
  camera_privacy_switch_state_ = state;
  // Issue a notification if camera is disabled by HW switch, but not by the SW
  // switch and there is multiple cameras.
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

cros::mojom::CameraPrivacySwitchState
CameraPrivacySwitchController::HWSwitchState() const {
  return camera_privacy_switch_state_;
}

void CameraPrivacySwitchController::ActiveApplicationsChanged(
    bool application_added) {
  if (application_added) {
    active_applications_using_camera_count_++;
  } else {
    DCHECK_GT(active_applications_using_camera_count_, 0);
    active_applications_using_camera_count_--;
  }

  if (GetUserSwitchPreference() != CameraSWPrivacySwitchSetting::kDisabled) {
    return;
  }

  if (active_applications_using_camera_count_ == 0 &&
      camera_used_while_deactivated_) {
    camera_used_while_deactivated_ = false;
    GetPrivacyHubNotificationController()->RemoveSensorDisabledNotification(
        PrivacyHubNotificationController::Sensor::kCamera);
  } else if (application_added) {
    camera_used_while_deactivated_ = true;
    GetPrivacyHubNotificationController()->ShowSensorDisabledNotification(
        PrivacyHubNotificationController::Sensor::kCamera);
  } else {
    GetPrivacyHubNotificationController()->UpdateSensorDisabledNotification(
        PrivacyHubNotificationController::Sensor::kCamera);
  }
}

}  // namespace ash
