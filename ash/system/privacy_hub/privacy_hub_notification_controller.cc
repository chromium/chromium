// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/microphone_mute/microphone_mute_notification_controller.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

PrivacyHubNotificationController::PrivacyHubNotificationController(
    MicrophoneMuteNotificationController*
        microphone_mute_notification_controller)
    : microphone_mute_notification_controller_(
          microphone_mute_notification_controller) {}

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
  if (microphone_mute_notification_controller_) {
    microphone_mute_notification_controller_->MaybeShowNotification(
        message_center::NotificationPriority::DEFAULT_PRIORITY,
        /*recreate=*/true);
  }
}

void PrivacyHubNotificationController::ShowLocationDisabledNotification()
    const {
  // TODO(b/242684137) Location is a WIP and doesn't have notifications yet but
  // will get them in future CLs.
}

void PrivacyHubNotificationController::
    ShowMicrophoneAndCameraDisabledNotification() {
  message_center::RichNotificationData notification_data;
  notification_data.buttons.emplace_back(l10n_util::GetStringUTF16(
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_BUTTON));

  message_center::MessageCenter::Get()->AddNotification(
      CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kCombinedNotificationId,
          l10n_util::GetStringUTF16(
              IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE),
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
      message_center->RemoveNotification(
          MicrophoneMuteNotificationController::kNotificationId,
          /*by_user=*/false);

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
      message_center->RemoveNotification(
          MicrophoneMuteNotificationController::kNotificationId,
          /*by_user=*/false);
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
  message_center::MessageCenter::Get()->RemoveNotification(
      PrivacyHubNotificationController::kCombinedNotificationId,
      /*by_user=*/true);

  if (!button_index) {
    ignore_new_combinable_notifications_ = true;
    // TODO(b/253165478) Clicking on any of the sensor notifications outside
    // the button will open Privacy Hub in a future CL.
    return;
  }

  MicrophoneMuteNotificationController::SetAndLogMicrophoneMute(false);
  CameraPrivacySwitchController::SetAndLogCameraPreferenceFromNotification(
      true);
}

}  // namespace ash
