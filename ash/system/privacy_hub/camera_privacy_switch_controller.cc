// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "base/bind.h"
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
    : switch_api_(std::make_unique<VCDPrivacyAdapter>()),
      camera_privacy_switch_state_(media::CameraHalDispatcherImpl::GetInstance()
                                       ->AddCameraPrivacySwitchObserver(this))

{
  media::CameraHalDispatcherImpl::GetInstance()->AddActiveClientObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);
}

CameraPrivacySwitchController::~CameraPrivacySwitchController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  media::CameraHalDispatcherImpl::GetInstance()->RemoveActiveClientObserver(
      this);
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
  // To ensure consistent values between the user pref and camera backend
  OnPreferenceChanged(prefs::kUserCameraAllowed);
}

void CameraPrivacySwitchController::OnPreferenceChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kUserCameraAllowed);
  const CameraSWPrivacySwitchSetting pref_val = GetUserSwitchPreference();
  switch_api_->SetCameraSWPrivacySwitch(pref_val);
  ClearSWSwitchNotifications();
  if (is_camera_active_ &&
      pref_val == CameraSWPrivacySwitchSetting::kDisabled) {
    // Show notification in case we switch off the camera when the camera is
    // used by an app.
    ShowCameraOffNotification();
  }
}

CameraSWPrivacySwitchSetting
CameraPrivacySwitchController::GetUserSwitchPreference() {
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

void CameraPrivacySwitchController::OnCameraHWPrivacySwitchStatusChanged(
    int32_t camera_id,
    cros::mojom::CameraPrivacySwitchState state) {
  camera_privacy_switch_state_ = state;
  PrivacyHubDelegate* const frontend =
      Shell::Get()->privacy_hub_controller()->frontend();
  if (frontend) {
    // This event can be received before the frontend delegate is registered
    frontend->CameraHardwareToggleChanged(state);
  }
  // Issue a notification if camera is disabled by HW switch, but not by the SW
  // switch
  if (state == cros::mojom::CameraPrivacySwitchState::ON &&
      GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kEnabled) {
    ShowHWCameraSwitchOffSWCameraSwitchOnNotification();
  }
}

cros::mojom::CameraPrivacySwitchState
CameraPrivacySwitchController::HWSwitchState() const {
  return camera_privacy_switch_state_;
}

void CameraPrivacySwitchController::ShowCameraOffNotification() {
  ShowNotification(/*action_enables_camera=*/true,
                   kPrivacyHubCameraOffNotificationId,
                   IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE,
                   IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE,
                   ash::NotificationCatalogName::kPrivacyHubCamera);
}

void CameraPrivacySwitchController::
    ShowHWCameraSwitchOffSWCameraSwitchOnNotification() {
  ShowNotification(
      /*action_enables_camera=*/false,
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId,
      IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_TITLE,
      IDS_PRIVACY_HUB_WANT_TO_TURN_OFF_CAMERA_NOTIFICATION_MESSAGE,
      NotificationCatalogName::kPrivacyHubHWCameraSwitchOffSWCameraSwitchOn);
}

void CameraPrivacySwitchController::ShowNotification(
    bool action_enables_camera,
    const char* kNotificationId,
    const int notification_title_id,
    const int notification_message_id,
    const NotificationCatalogName catalog) {
  message_center::RichNotificationData notification_data;
  notification_data.pinned = true;
  notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      action_enables_camera ? IDS_PRIVACY_HUB_TURN_ON_CAMERA_ACTION_BUTTON
                            : IDS_PRIVACY_HUB_TURN_OFF_CAMERA_ACTION_BUTTON));

  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](bool camera_enabled, absl::optional<int> button_index) {
                if (!button_index) {
                  // Click on the notification body is no-op.
                  return;
                }
                PrefService* const pref_service =
                    Shell::Get()->session_controller()->GetActivePrefService();
                if (pref_service) {
                  pref_service->SetBoolean(prefs::kUserCameraAllowed,
                                           camera_enabled);
                  privacy_hub_metrics::LogCameraEnabledFromNotification(
                      camera_enabled);
                }
              },
              action_enables_camera));

  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           /*by_user=*/false);
  message_center::MessageCenter::Get()->AddNotification(
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          l10n_util::GetStringUTF16(notification_title_id),
          l10n_util::GetStringUTF16(notification_message_id),
          /*display_source=*/std::u16string(),
          /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId,
              catalog),
          notification_data, std::move(delegate),
          vector_icons::kVideocamOffIcon,
          message_center::SystemNotificationWarningLevel::NORMAL));
}

void CameraPrivacySwitchController::ClearSWSwitchNotifications() {
  constexpr std::array kNotificationIds = {
      kPrivacyHubCameraOffNotificationId,
      kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId};
  message_center::MessageCenter* const message_center =
      message_center::MessageCenter::Get();
  if (!message_center) {
    return;
  }
  for (const char* notification_id : kNotificationIds) {
    message_center->RemoveNotification(notification_id,
                                       /*by_user=*/false);
  }
}

void CameraPrivacySwitchController::OnActiveClientChange(
    cros::mojom::CameraClientType type,
    bool is_active) {
  is_camera_active_ = is_active;

  if (is_active) {
    if (GetUserSwitchPreference() == CameraSWPrivacySwitchSetting::kDisabled)
      ShowCameraOffNotification();
  }
}

}  // namespace ash
