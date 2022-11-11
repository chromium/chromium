// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_

#include "ash/ash_export.h"

namespace ash::privacy_hub_metrics {

// These values are persisted to logs and should not be renumbered or re-used.
// Keep in sync with PrivacyHubNavigationOrigin in
// tools/metrics/histograms/enums.xml and
// c/b/resources/settings/chromeos/os_privacy_page/privacy_hub_page.js.
enum class PrivacyHubNavigationOrigin {
  kSystemSettings = 0,
  kNotification = 1,
  kMaxValue = kNotification
};

static constexpr char kPrivacyHubMicrophoneEnabledFromSettingsHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Settings.Enabled";
static constexpr char kPrivacyHubMicrophoneEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Notification.Enabled";
static constexpr char kPrivacyHubCameraEnabledFromSettingsHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Settings.Enabled";
static constexpr char kPrivacyHubCameraEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Notification.Enabled";
static constexpr char kPrivacyHubOpenedHistogram[] =
    "ChromeOS.PrivacyHub.Opened";

// Report microphone mute events from system and notifications.
ASH_EXPORT void LogMicrophoneEnabledFromSettings(bool enabled);
ASH_EXPORT void LogMicrophoneEnabledFromNotification(bool enabled);

// Report camera mute events from system and notifications.
ASH_EXPORT void LogCameraEnabledFromSettings(bool enabled);
ASH_EXPORT void LogCameraEnabledFromNotification(bool enabled);

// Report that Privacy Hub has been opened from a notification.
ASH_EXPORT void LogPrivacyHubOpenedFromNotification();

}  // namespace ash::privacy_hub_metrics

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
