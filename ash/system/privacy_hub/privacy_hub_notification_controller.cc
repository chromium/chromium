// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/system_notification_controller.h"
#include "base/notreached.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {
using Sensor = SensorDisabledNotificationDelegate::Sensor;

namespace {

constexpr char kLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=privacy_hub";

void LogInvalidSensor(const Sensor sensor) {
  NOTREACHED() << "Invalid sensor: "
               << static_cast<std::underlying_type_t<Sensor>>(sensor);
}

}  // namespace

PrivacyHubNotificationController::PrivacyHubNotificationController() {
  auto camera_notification_descriptor = PrivacyHubNotificationDescriptor(
      SensorSet{Sensor::kCamera}, IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE,
      std::vector<int>{IDS_PRIVACY_HUB_TURN_ON_CAMERA_ACTION_BUTTON},
      std::vector<int>{
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE,
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES});

  auto microphone_notification_descriptor = PrivacyHubNotificationDescriptor(
      SensorSet{Sensor::kMicrophone},
      IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE,
      std::vector<int>{IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON},
      std::vector<int>{
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE,
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES});

  auto combined_notification_descriptor = PrivacyHubNotificationDescriptor(
      SensorSet{Sensor::kCamera, Sensor::kMicrophone},
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE,
      std::vector<int>{
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_BUTTON},
      std::vector<int>{
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE,
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES});

  combined_notification_descriptor.delegate()->SetSecondButtonCallback(
      base::BindRepeating(
          &PrivacyHubNotificationController::OpenPrivacyHubSettingsPage));

  combined_notification_ = std::make_unique<PrivacyHubNotification>(
      kCombinedNotificationId, NotificationCatalogName::kPrivacyHubMicAndCamera,
      std::vector<PrivacyHubNotificationDescriptor>{
          camera_notification_descriptor, microphone_notification_descriptor,
          combined_notification_descriptor});

  microphone_hw_switch_notification_ = std::make_unique<PrivacyHubNotification>(
      kMicrophoneHardwareSwitchNotificationId,
      NotificationCatalogName::kMicrophoneMute,
      PrivacyHubNotificationDescriptor{
          SensorSet{Sensor::kMicrophone},
          IDS_MICROPHONE_MUTED_BY_HW_SWITCH_NOTIFICATION_TITLE,
          std::vector<int>{IDS_ASH_LEARN_MORE},
          std::vector<int>{
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE,
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES},
          base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
              base::BindRepeating(
                  PrivacyHubNotificationController::OpenSupportUrl,
                  Sensor::kMicrophone))});

  PrivacyHubNotificationDescriptor geolocation_notification_descriptor(
      SensorSet{Sensor::kLocation},
      IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_TITLE,
      std::vector<int>{IDS_PRIVACY_HUB_TURN_ON_GEOLOCATION_ACTION_BUTTON,
                       IDS_PRIVACY_HUB_TURN_ON_GEOLOCATION_LEARN_MORE_BUTTON},
      std::vector<int>{
          IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_MESSAGE,
          IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES});
  geolocation_notification_descriptor.delegate()->SetSecondButtonCallback(
      base::BindRepeating(PrivacyHubNotificationController::OpenSupportUrl,
                          Sensor::kLocation));
  geolocation_notification_ = std::make_unique<PrivacyHubNotification>(
      kGeolocationSwitchNotificationId,
      NotificationCatalogName::kGeolocationSwitch,
      std::move(geolocation_notification_descriptor));
}

PrivacyHubNotificationController::~PrivacyHubNotificationController() = default;

// static
PrivacyHubNotificationController* PrivacyHubNotificationController::Get() {
  SystemNotificationController* system_notification_controller =
      Shell::Get()->system_notification_controller();
  return system_notification_controller
             ? system_notification_controller->privacy_hub()
             : nullptr;
}

void PrivacyHubNotificationController::ShowSoftwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      // Microphone software switch notification will be displayed now. If the
      // hardware switch notification is still not cleared, clear it first.
      microphone_hw_switch_notification_->Hide();
      [[fallthrough]];
    }
    case Sensor::kCamera: {
      AddSensor(sensor);
      combined_notification_->Show();
      break;
    }
    case Sensor::kLocation: {
      geolocation_notification_->Show();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::RemoveSoftwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kCamera: {
      [[fallthrough]];
    }
    case Sensor::kMicrophone: {
      RemoveSensor(sensor);
      if (!sensors_.Empty()) {
        combined_notification_->Update();
      } else {
        combined_notification_->Hide();
      }
      break;
    }
    case Sensor::kLocation: {
      geolocation_notification_->Hide();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::UpdateSoftwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kCamera: {
      [[fallthrough]];
    }
    case Sensor::kMicrophone: {
      combined_notification_->Update();
      break;
    }
    case Sensor::kLocation: {
      geolocation_notification_->Update();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

bool PrivacyHubNotificationController::
    IsSoftwareSwitchNotificationDisplayedForSensor(Sensor sensor) {
  return combined_notification_->IsShown() && sensors_.Has(sensor);
}

void PrivacyHubNotificationController::
    SetPriorityForMicrophoneHardwareNotification(
        message_center::NotificationPriority priority) {
  microphone_hw_switch_notification_->SetPriority(priority);
}

void PrivacyHubNotificationController::ShowHardwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      RemoveSensor(sensor);
      if (!sensors_.Empty()) {
        combined_notification_->Update();
      } else {
        // As the hardware switch notification for microphone will be displayed
        // now, remove the sw switch notification.
        combined_notification_->Hide();
      }
      microphone_hw_switch_notification_->Show();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::RemoveHardwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      microphone_hw_switch_notification_->Hide();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::UpdateHardwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      microphone_hw_switch_notification_->Update();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::OpenPrivacyHubSettingsPage() {
  privacy_hub_metrics::LogPrivacyHubOpenedFromNotification();
  Shell::Get()->system_tray_model()->client()->ShowPrivacyHubSettings();
}

void PrivacyHubNotificationController::OpenSupportUrl(Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone:
      privacy_hub_metrics::LogPrivacyHubLearnMorePageOpened(
          privacy_hub_metrics::PrivacyHubLearnMoreSensor::kMicrophone);
      break;
    case Sensor::kCamera:
      privacy_hub_metrics::LogPrivacyHubLearnMorePageOpened(
          privacy_hub_metrics::PrivacyHubLearnMoreSensor::kCamera);
      break;
    case Sensor::kLocation:
      privacy_hub_metrics::LogPrivacyHubLearnMorePageOpened(
          privacy_hub_metrics::PrivacyHubLearnMoreSensor::kGeolocation);
      return;
  }
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kLearnMoreUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

// static
void PrivacyHubNotificationController::
    SetAndLogSensorPreferenceFromNotification(Sensor sensor,
                                              const bool enabled) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service) {
    return;
  }

  const char* pref_name = nullptr;
  switch (sensor) {
    case Sensor::kCamera: {
      pref_name = prefs::kUserCameraAllowed;
      break;
    }
    case Sensor::kMicrophone: {
      pref_name = prefs::kUserMicrophoneAllowed;
      break;
    }
    case Sensor::kLocation: {
      pref_name = prefs::kUserGeolocationAllowed;
      break;
    }
  }
  CHECK(pref_name);

  pref_service->SetBoolean(pref_name, enabled);
  privacy_hub_metrics::LogSensorEnabledFromNotification(sensor, enabled);
}

void PrivacyHubNotificationController::AddSensor(Sensor sensor) {
  sensors_.Put(sensor);
  combined_notification_->SetSensors(sensors_);
}

void PrivacyHubNotificationController::RemoveSensor(Sensor sensor) {
  sensors_.Remove(sensor);
  combined_notification_->SetSensors(sensors_);
}

}  // namespace ash
