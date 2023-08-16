// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"

namespace ash {

// A class managing when to show notifications for microphone, camera and
// geolocation to the user or combining them if necessary.
class ASH_EXPORT PrivacyHubNotificationController {
  using Sensor = SensorDisabledNotificationDelegate::Sensor;
  using SensorSet = SensorDisabledNotificationDelegate::SensorSet;

 public:
  PrivacyHubNotificationController();
  PrivacyHubNotificationController(const PrivacyHubNotificationController&) =
      delete;
  PrivacyHubNotificationController& operator=(
      const PrivacyHubNotificationController&) = delete;
  ~PrivacyHubNotificationController();

  // Gets the singleton instance that lives within `Shell` if available.
  static PrivacyHubNotificationController* Get();

  // Called by any sensor system when a software switch notification for
  // `sensor` should be shown to the user.
  void ShowSoftwareSwitchNotification(Sensor sensor);

  // Called by any sensor system when a software switch notification for
  // `sensor` should be removed from the notification center and popups.
  void RemoveSoftwareSwitchNotification(Sensor sensor);

  // Called by any sensor system when a software switch notification for
  // `sensor` should be updated, for example, when an application stops
  // accessing `sensor`.
  void UpdateSoftwareSwitchNotification(Sensor sensor);

  // Checks if a sensor-related notification is shown.
  bool IsSoftwareSwitchNotificationDisplayedForSensor(Sensor sensor);

  // Allows to alter priority for the upcoming microphone hw notifications.
  void SetPriorityForMicrophoneHardwareNotification(
      message_center::NotificationPriority priority);

  // Called by any sensor system when a hardware switch notification for
  // `sensor` should be shown to the user.
  void ShowHardwareSwitchNotification(Sensor sensor);

  // Called by any sensor system when a hardware switch notification for
  // `sensor` should be removed from the notification center and popups.
  void RemoveHardwareSwitchNotification(Sensor sensor);

  // Called by any sensor system when a hardware switch notification for
  // `sensor` should be updated, for example, when an application stops
  // accessing `sensor`.
  void UpdateHardwareSwitchNotification(Sensor sensor);

  // This same id will be used for
  // - microphone software switch notification
  // - camera software switch notification
  // - microphone and camera combined notification
  static constexpr const char kCombinedNotificationId[] =
      "ash.system.privacy_hub.enable_microphone_and/or_camera";

  static constexpr const char kMicrophoneHardwareSwitchNotificationId[] =
      "ash://microphone_hardware_mute";

  static constexpr const char kGeolocationSwitchNotificationId[] =
      "ash://geolocation_switch";

  // Open the Privacy Hub settings page and log that this interaction came from
  // a notification.
  static void OpenPrivacyHubSettingsPage();

  // Open the support page for Privacy Hub and logs the interaction together
  // with what `sensor` was in use by the user.
  static void OpenSupportUrl(Sensor sensor);

  // Set the appropriate pref to the value of `enabled` and log the
  // interaction from a notification.
  static void SetAndLogSensorPreferenceFromNotification(
      SensorDisabledNotificationDelegate::Sensor sensor,
      const bool enabled);

  // Sets the `SensorDisabledNotification` delegate. The original one is
  // returned.
  std::unique_ptr<SensorDisabledNotificationDelegate>
  SetSensorDisabledNotificationDelegate(
      std::unique_ptr<SensorDisabledNotificationDelegate> delegate);

  // Retrieves the `SensorDisabledNotificationDelegate` or nullptr.
  SensorDisabledNotificationDelegate* sensor_disabled_notification_delegate();

  PrivacyHubNotification* combined_notification_for_test() {
    return combined_notification_.get();
  }

 private:
  void AddSensor(SensorDisabledNotificationDelegate::Sensor sensor);
  void RemoveSensor(SensorDisabledNotificationDelegate::Sensor sensor);

  const SensorSet combinable_sensors_{Sensor::kMicrophone, Sensor::kCamera};

  // `combined_notification_` will be displayed for the sensors which are
  // currently in `sensors_`. Only `combinable_sensors_` can be in `sensors_`.
  SensorSet sensors_;

  // This PrivacyHubNotification object will be used to display
  // - microphone software switch notification
  // - camera software switch notification
  // - microphone and camera combined notification
  // - geolocation software switch notification
  std::unique_ptr<PrivacyHubNotification> combined_notification_;
  std::unique_ptr<PrivacyHubNotification> microphone_hw_switch_notification_;
  std::unique_ptr<PrivacyHubNotification> geolocation_notification_;
  std::unique_ptr<SensorDisabledNotificationDelegate>
      sensor_disabled_notification_delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_
