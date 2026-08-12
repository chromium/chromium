// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_

#include "ash/ash_export.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"

namespace ash::privacy_hub_metrics {
using Sensor = SensorDisabledNotificationDelegate::Sensor;

// These values are persisted to logs and should not be renumbered or reused.
// Keep in sync with PrivacyHubNavigationOrigin in
// tools/metrics/histograms/enums.xml and
// c/b/resources/ash/settings/os_privacy_page/privacy_hub_subpage.js.
// LINT.IfChange(PrivacyHubNavigationOrigin)
enum class PrivacyHubNavigationOrigin {
  kSystemSettings = 0,
  kNotification = 1,
  kMaxValue = kNotification
};
// LINT.ThenChange(
//   //chrome/browser/resources/ash/settings/os_privacy_page/privacy_hub_subpage.ts:PrivacyHubNavigationOrigin,
//   //tools/metrics/histograms/metadata/chromeos/enums.xml:PrivacyHubNavigationOrigin
// )

// These values are persisted to logs and should not be renumbered or reused.
enum class PrivacyHubLearnMoreSensor {
  kMicrophone = 0,
  kCamera = 1,
  kGeolocation = 2,
  kMaxValue = kGeolocation
};

inline constexpr char kPrivacyHubMicrophoneEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Notification.Enabled";
inline constexpr char kPrivacyHubCameraEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Notification.Enabled";
inline constexpr char
    kPrivacyHubGeolocationAccessLevelChangedFromNotification[] =
        "ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged."
        "LocationPermissionNotification";
inline constexpr char kPrivacyHubOpenedHistogram[] =
    "ChromeOS.PrivacyHub.Opened";
inline constexpr char kPrivacyHubLearnMorePageOpenedHistogram[] =
    "ChromeOS.PrivacyHub.LearnMorePage.Opened";

// Report sensor events from notifications.
ASH_EXPORT void LogSensorEnabledFromNotification(Sensor sensor, bool enabled);

// Report that Privacy Hub has been opened from a notification.
ASH_EXPORT void LogPrivacyHubOpenedFromNotification();

// Report that the user opened the learn more page for Privacy Hub from a
// microphone notification.
ASH_EXPORT void LogPrivacyHubLearnMorePageOpened(
    PrivacyHubLearnMoreSensor sensor);

}  // namespace ash::privacy_hub_metrics

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
