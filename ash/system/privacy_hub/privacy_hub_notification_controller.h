// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/containers/enum_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class MicrophoneMuteNotificationController;

// A class managing when to show notifications for microphone, camera and
// geolocation to the user or combining them if necessary.
class ASH_EXPORT PrivacyHubNotificationController {
 public:
  enum class Sensor {
    kCamera,
    kMin = kCamera,
    kLocation,
    kMicrophone,
    kMax = kMicrophone
  };

  using SensorEnumSet = base::EnumSet<Sensor, Sensor::kMin, Sensor::kMax>;

  explicit PrivacyHubNotificationController(
      MicrophoneMuteNotificationController*
          microphone_mute_notification_controller);

  PrivacyHubNotificationController(const PrivacyHubNotificationController&) =
      delete;
  PrivacyHubNotificationController& operator=(
      const PrivacyHubNotificationController&) = delete;

  ~PrivacyHubNotificationController();

  // Called by any sensor system when a notification for `sensor`
  // should be shown to the user.
  void ShowSensorDisabledNotification(Sensor sensor);

  // Called by any sensor system when a notification for `sensor`
  // should be removed from the notification center and popups.
  void RemoveSensorDisabledNotification(Sensor sensor);

  static constexpr const char kCombinedNotificationId[] =
      "ash.system.privacy_hub.enable_microphone_and_camera";

  // Open the Privacy Hub settings page and log that this interaction came from
  // a notification.
  static void OpenPrivacyHubSettingsPage();

 private:
  // Display the camera disabled by software switch notification.
  void ShowCameraDisabledNotification() const;

  // TODO(b/242684137) Location is a WIP and doesn't have notifications yet but
  // will get them in future CLs.
  void ShowLocationDisabledNotification() const;

  // Display the microphone is disabled notification.
  void ShowMicrophoneDisabledNotification() const;

  // Constructs the notification message for the combined notification,
  // containing the app names that use mic and camera sensors. The notification
  // has different format, depending on the number of the apps.
  std::u16string GenerateMicrophoneAndCameraDisabledNotificationMessage();

  // Display a combined notification when camera software switch and
  // microphone are disabled.
  void ShowMicrophoneAndCameraDisabledNotification();

  // Show all notifications that are currently active and combine them if
  // necessary. From the `changed_sensor` in combination with `sensors_`,
  // `combinable_sensors_` and `ignore_new_combinable_notifications_` the
  // appropriate notification will be shown and unnecessary notifications
  // removed if necessary.
  void ShowAllActiveNotifications(Sensor changed_sensor);

  void HandleNotificationClicked(absl::optional<int> button_index);

  const SensorEnumSet combinable_sensors_{Sensor::kMicrophone, Sensor::kCamera};
  // Flag to keep track if the user opened the settings page and don't show
  // them new notifications of sensors that can be combined or the combined
  // notification until the number of active uses falls to 0.
  bool ignore_new_combinable_notifications_{false};
  const base::raw_ptr<MicrophoneMuteNotificationController>
      microphone_mute_notification_controller_;
  SensorEnumSet sensors_;
  base::WeakPtrFactory<PrivacyHubNotificationController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_
