// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

void SetAndLogMicrophoneMute(const bool muted) {
  CrasAudioHandler::Get()->SetInputMute(
      muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
  privacy_hub_metrics::LogMicrophoneEnabledFromNotification(!muted);
}

constexpr char kLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=privacy_hub";

}  // namespace

PrivacyHubNotificationController::PrivacyHubNotificationController() {
  sw_notifications_.emplace(
      Sensor::kCamera,
      std::make_unique<PrivacyHubNotification>(
          kPrivacyHubCameraOffNotificationId,
          IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE,
          PrivacyHubNotification::MessageIds{
              IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE,
              IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
              IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES},
          PrivacyHubNotification::SensorSet{
              SensorDisabledNotificationDelegate::Sensor::kCamera},
          base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
              base::BindRepeating([]() {
                CameraPrivacySwitchController::
                    SetAndLogCameraPreferenceFromNotification(true);
              })),
          ash::NotificationCatalogName::kPrivacyHubCamera,
          IDS_PRIVACY_HUB_TURN_ON_CAMERA_ACTION_BUTTON));

  sw_notifications_.emplace(
      Sensor::kMicrophone,
      std::make_unique<PrivacyHubNotification>(
          MicrophonePrivacySwitchController::kNotificationId,
          IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE,
          PrivacyHubNotification::MessageIds{
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE,
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES},
          PrivacyHubNotification::SensorSet{
              SensorDisabledNotificationDelegate::Sensor::kMicrophone},
          base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
              base::BindRepeating([]() { SetAndLogMicrophoneMute(false); })),
          ash::NotificationCatalogName::kMicrophoneMute,
          IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON));

  auto combined_delegate = base::MakeRefCounted<
      PrivacyHubNotificationClickDelegate>(base::BindRepeating([]() {
    SetAndLogMicrophoneMute(false);
    CameraPrivacySwitchController::SetAndLogCameraPreferenceFromNotification(
        true);
  }));
  combined_delegate->SetMessageClickCallback(base::BindRepeating(
      &PrivacyHubNotificationController::HandleNotificationMessageClicked,
      weak_ptr_factory_.GetWeakPtr()));

  combined_notification_ = std::make_unique<PrivacyHubNotification>(
      kCombinedNotificationId,
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE,
      PrivacyHubNotification::MessageIds{
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE,
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES},
      PrivacyHubNotification::SensorSet{
          SensorDisabledNotificationDelegate::Sensor::kCamera,
          SensorDisabledNotificationDelegate::Sensor::kMicrophone},
      combined_delegate, NotificationCatalogName::kPrivacyHubMicAndCamera,
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_BUTTON);
}

PrivacyHubNotificationController::~PrivacyHubNotificationController() = default;

void PrivacyHubNotificationController::ShowSensorDisabledNotification(
    const Sensor sensor) {
  sensors_.Put(sensor);

  ShowAllActiveNotifications(sensor);
}

void PrivacyHubNotificationController::RemoveSensorDisabledNotification(
    const Sensor sensor) {
  sensors_.Remove(sensor);

  if (!sensors_.HasAny(combinable_sensors_)) {
    ignore_new_combinable_notifications_ = false;
  }

  ShowAllActiveNotifications(sensor);
}

void PrivacyHubNotificationController::UpdateSensorDisabledNotification(
    const Sensor sensor) {
  sw_notifications_.at(sensor)->Update();
  combined_notification_->Update();
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
      LOG(DFATAL) << "Location doesn't have a learn more button";
      return;
  }
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kLearnMoreUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void PrivacyHubNotificationController::ShowAllActiveNotifications(
    const Sensor changed_sensor) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  DCHECK(message_center);

  if (combinable_sensors_.Has(changed_sensor)) {
    combined_notification_->Hide();

    if (ignore_new_combinable_notifications_)
      return;

    if (sensors_.HasAll(combinable_sensors_)) {
      for (Sensor sensor : combinable_sensors_) {
        sw_notifications_.at(sensor)->Hide();
      }

      combined_notification_->Show();

      return;
    }
  }

  // Remove the notification for the current sensor in case the sensor is
  // no longer active it won't be shown again in the for loop later.
  // The other case where the sensor is added (again) to the set this
  // (re)surfaces the notification, e.g. because a different app now wants to
  // access the sensor.
  sw_notifications_.at(changed_sensor)->Hide();

  for (const Sensor active_sensor : sensors_) {
    sw_notifications_.at(active_sensor)->Show();
  }
}

void PrivacyHubNotificationController::HandleNotificationMessageClicked() {
  ignore_new_combinable_notifications_ = true;
}

}  // namespace ash
