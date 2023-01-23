// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"

namespace ash {

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

  PrivacyHubNotificationController();
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

  // Called by any sensor system when a notification for `sensor` should be
  // updated, for example, when an application stops accessing `sensor`.
  void UpdateSensorDisabledNotification(Sensor sensor);

  static constexpr const char kCombinedNotificationId[] =
      "ash.system.privacy_hub.enable_microphone_and_camera";

  // Open the Privacy Hub settings page and log that this interaction came from
  // a notification.
  static void OpenPrivacyHubSettingsPage();

  // Open the support page for Privacy Hub and logs the interaction together
  // with what `sensor` was in use by the user.
  static void OpenSupportUrl(Sensor sensor);

 private:
  // Show all notifications that are currently active and combine them if
  // necessary. From the `changed_sensor` in combination with `sensors_`,
  // `combinable_sensors_` and `ignore_new_combinable_notifications_` the
  // appropriate notification will be shown and unnecessary notifications
  // removed if necessary.
  void ShowAllActiveNotifications(Sensor changed_sensor);

  void HandleNotificationMessageClicked();

  const SensorEnumSet combinable_sensors_{Sensor::kMicrophone, Sensor::kCamera};
  // Flag to keep track if the user opened the settings page and don't show
  // them new notifications of sensors that can be combined or the combined
  // notification until the number of active uses falls to 0.
  bool ignore_new_combinable_notifications_{false};
  SensorEnumSet sensors_;
  std::unique_ptr<PrivacyHubNotification> combined_notification_;
  base::flat_map<Sensor, std::unique_ptr<PrivacyHubNotification>>
      sw_notifications_;
  base::WeakPtrFactory<PrivacyHubNotificationController> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_NOTIFICATION_CONTROLLER_H_
