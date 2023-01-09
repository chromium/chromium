// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include <iterator>

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

void SetAndLogMicrophoneMute(const bool muted) {
  CrasAudioHandler::Get()->SetInputMute(
      muted, CrasAudioHandler::InputMuteChangeMethod::kOther);
  privacy_hub_metrics::LogMicrophoneEnabledFromNotification(!muted);
}

}  // namespace

PrivacyHubNotificationController::PrivacyHubNotificationController()
    : microphone_notification_(std::make_unique<PrivacyHubNotification>(
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
          IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON)) {}

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

void PrivacyHubNotificationController::OpenPrivacyHubSettingsPage() {
  privacy_hub_metrics::LogPrivacyHubOpenedFromNotification();
  Shell::Get()->system_tray_model()->client()->ShowPrivacyHubSettings();
}

void PrivacyHubNotificationController::ShowCameraDisabledNotification() const {
  if (Shell::Get()->privacy_hub_controller()) {
    Shell::Get()
        ->privacy_hub_controller()
        ->camera_controller()
        .ShowCameraOffNotification();
  }
}

void PrivacyHubNotificationController::ShowMicrophoneDisabledNotification()
    const {
  // TODO(cschlosser) Clean this up in the follow up CL to be consistent across
  // sensors.
  microphone_notification_->Show();
}

void PrivacyHubNotificationController::ShowLocationDisabledNotification()
    const {
  // TODO(b/242684137) Location is a WIP and doesn't have notifications yet but
  // will get them in future CLs.
}

std::u16string PrivacyHubNotificationController::
    GenerateMicrophoneAndCameraDisabledNotificationMessage() {
  SensorDisabledNotificationDelegate* delegate =
      SensorDisabledNotificationDelegate::Get();
  DCHECK(delegate);

  auto camera_apps = delegate->GetAppsAccessingSensor(
      SensorDisabledNotificationDelegate::Sensor::kCamera);
  auto mic_apps = delegate->GetAppsAccessingSensor(
      SensorDisabledNotificationDelegate::Sensor::kMicrophone);

  // Take mathematical union of the apps. Two different apps can have the same
  // short name, we'll display it only once.
  std::set<std::u16string> all_apps;
  all_apps.insert(camera_apps.begin(), camera_apps.end());
  all_apps.insert(mic_apps.begin(), mic_apps.end());

  if (all_apps.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
        *all_apps.begin());
  }
  if (all_apps.size() == 2) {
    return l10n_util::GetStringFUTF16(
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES,
        *all_apps.begin(), *std::next(all_apps.begin()));
  }

  // Return generic text by default.
  return l10n_util::GetStringUTF16(
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE);
}

void PrivacyHubNotificationController::
    ShowMicrophoneAndCameraDisabledNotification() {
  message_center::RichNotificationData notification_data;
  notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_BUTTON));
  notification_data.remove_on_click = true;

  message_center::MessageCenter::Get()->AddNotification(
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kCombinedNotificationId,
          l10n_util::GetStringUTF16(
              IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
          GenerateMicrophoneAndCameraDisabledNotificationMessage(),
          /*display_source=*/std::u16string(),
          /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kCombinedNotificationId,
              NotificationCatalogName::kPrivacyHubMicAndCamera),
          notification_data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(
                  &PrivacyHubNotificationController::HandleNotificationClicked,
                  weak_ptr_factory_.GetWeakPtr())),
          vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL));
}

void PrivacyHubNotificationController::ShowAllActiveNotifications(
    const Sensor changed_sensor) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  DCHECK(message_center);

  if (combinable_sensors_.Has(changed_sensor)) {
    message_center->RemoveNotification(kCombinedNotificationId,
                                       /*by_user=*/false);

    if (ignore_new_combinable_notifications_)
      return;

    if (sensors_.HasAll(combinable_sensors_)) {
      message_center->RemoveNotification(kPrivacyHubCameraOffNotificationId,
                                         /*by_user=*/false);
      microphone_notification_->Hide();

      ShowMicrophoneAndCameraDisabledNotification();

      return;
    }
  }

  // Remove the notification for the current sensor in case the sensor is
  // no longer active it won't be shown again in the for loop later.
  // The other case where the sensor is added (again) to the set this
  // (re)surfaces the notification, e.g. because a different app now wants to
  // access the sensor.
  switch (changed_sensor) {
    case Sensor::kCamera:
      message_center->RemoveNotification(kPrivacyHubCameraOffNotificationId,
                                         /*by_user=*/false);
      break;
    case Sensor::kLocation:
      // TODO(b/242684137) Remove location notification as well.
      break;
    case Sensor::kMicrophone:
      microphone_notification_->Hide();
      break;
  }

  for (const Sensor active_sensor : sensors_) {
    switch (active_sensor) {
      case Sensor::kCamera:
        ShowCameraDisabledNotification();
        break;
      case Sensor::kLocation:
        ShowLocationDisabledNotification();
        break;
      case Sensor::kMicrophone:
        ShowMicrophoneDisabledNotification();
        break;
    }
  }
}

void PrivacyHubNotificationController::HandleNotificationClicked(
    absl::optional<int> button_index) {
  if (!button_index) {
    ignore_new_combinable_notifications_ = true;
    OpenPrivacyHubSettingsPage();
    return;
  }

  SetAndLogMicrophoneMute(false);
  CameraPrivacySwitchController::SetAndLogCameraPreferenceFromNotification(
      true);
}

}  // namespace ash
