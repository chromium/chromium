// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/system_notification_controller.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

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
    : switch_api_(std::make_unique<VCDPrivacyAdapter>())

{
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

  if (message_center::MessageCenter* const message_center =
          message_center::MessageCenter::Get()) {
    message_center->RemoveNotification(
        kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId,
        /*by_user=*/false);
  }

  if (active_applications_using_camera_count_ == 0)
    return;

  if (pref_val == CameraSWPrivacySwitchSetting::kDisabled) {
    camera_used_while_deactivated_ = true;
    Shell::Get()
        ->system_notification_controller()
        ->privacy_hub()
        ->ShowSensorDisabledNotification(
            PrivacyHubNotificationController::Sensor::kCamera);
  } else {
    camera_used_while_deactivated_ = false;
    Shell::Get()
        ->system_notification_controller()
        ->privacy_hub()
        ->RemoveSensorDisabledNotification(
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
  PrivacyHubDelegate* const frontend =
      Shell::Get()->privacy_hub_controller()->frontend();
  if (frontend) {
    // This event can be received before the frontend delegate is registered
    frontend->CameraHardwareToggleChanged(state);
  }
  // Issue a notification if camera is disabled by HW switch, but not by the SW
  // switch and there is multiple cameras.
  if (state == cros::mojom::CameraPrivacySwitchState::ON &&
      GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kEnabled &&
      camera_count_ > 1) {
    ShowHWCameraSwitchOffSWCameraSwitchOnNotification();
  }
  if (state == cros::mojom::CameraPrivacySwitchState::OFF) {
    // Clear the notification that might have been displayed earlier
    message_center::MessageCenter::Get()->RemoveNotification(
        kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId,
        /*by_user=*/false);
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

std::u16string
CameraPrivacySwitchController::GetCameraOffNotificationMessage() {
  auto* sensor_disabled_notification_delegate =
      SensorDisabledNotificationDelegate::Get();
  std::vector<std::u16string> app_names =
      sensor_disabled_notification_delegate->GetAppsAccessingSensor(
          SensorDisabledNotificationDelegate::Sensor::kCamera);

  if (app_names.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
        app_names[0]);
  } else if (app_names.size() == 2) {
    return l10n_util::GetStringFUTF16(
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
        app_names[0], app_names[1]);
  }

  return l10n_util::GetStringUTF16(
      IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE);
}

void CameraPrivacySwitchController::ShowCameraOffNotification() {
  ShowNotification(/*action_enables_camera=*/true,
                   kPrivacyHubCameraOffNotificationId,
                   IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE,
                   GetCameraOffNotificationMessage(),
                   ash::NotificationCatalogName::kPrivacyHubCamera);
}

void CameraPrivacySwitchController::
    ShowHWCameraSwitchOffSWCameraSwitchOnNotification() {
  ShowNotification(
      /*action_enables_camera=*/false,
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId,
      IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_TITLE,
      l10n_util::GetStringUTF16(
          IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_MESSAGE),
      NotificationCatalogName::kPrivacyHubHWCameraSwitchOffSWCameraSwitchOn);
}

void CameraPrivacySwitchController::ShowNotification(
    bool action_enables_camera,
    const char* kNotificationId,
    const int notification_title_id,
    const std::u16string& notification_message,
    const NotificationCatalogName catalog) {
  message_center::RichNotificationData notification_data;
  notification_data.pinned = false;
  notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      action_enables_camera ? IDS_PRIVACY_HUB_TURN_ON_CAMERA_ACTION_BUTTON
                            : IDS_PRIVACY_HUB_TURN_OFF_CAMERA_ACTION_BUTTON));
  notification_data.remove_on_click = true;

  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<
          message_center::HandleNotificationClickDelegate>(base::BindRepeating(
          [](bool camera_enabled, const char* notification_id,
             absl::optional<int> button_index) {
            if (!button_index) {
              PrivacyHubNotificationController::OpenPrivacyHubSettingsPage();
              return;
            }
            SetAndLogCameraPreferenceFromNotification(camera_enabled);
          },
          action_enables_camera, kNotificationId));

  message_center::MessageCenter::Get()->AddNotification(
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          l10n_util::GetStringUTF16(notification_title_id),
          notification_message,
          /*display_source=*/std::u16string(),
          /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId,
              catalog),
          notification_data, std::move(delegate), vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL));
}

void CameraPrivacySwitchController::ActiveApplicationsChanged(
    bool application_added) {
  if (application_added) {
    active_applications_using_camera_count_++;
  } else {
    DCHECK_GT(active_applications_using_camera_count_, 0);
    active_applications_using_camera_count_--;
  }

  // Notification should pop up when an application starts using the camera but
  // the camera is disabled by the software switch.
  if (application_added &&
      GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kDisabled) {
    camera_used_while_deactivated_ = true;
    Shell::Get()
        ->system_notification_controller()
        ->privacy_hub()
        ->ShowSensorDisabledNotification(
            PrivacyHubNotificationController::Sensor::kCamera);
  }

  // Remove existing software switch notification when no application is using
  // the camera anymore.
  if (active_applications_using_camera_count_ == 0 &&
      camera_used_while_deactivated_) {
    camera_used_while_deactivated_ = false;
    Shell::Get()
        ->system_notification_controller()
        ->privacy_hub()
        ->RemoveSensorDisabledNotification(
            PrivacyHubNotificationController::Sensor::kCamera);
  }
}

}  // namespace ash
