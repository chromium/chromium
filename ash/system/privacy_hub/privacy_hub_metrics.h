// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
#define ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_

#include "ash/ash_export.h"

namespace ash::privacy_hub_metrics {

static constexpr char kPrivacyHubMicrophoneEnabledFromSettingsHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Settings.Enabled";
static constexpr char kPrivacyHubMicrophoneEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Notification.Enabled";
static constexpr char kPrivacyHubCameraEnabledFromSettingsHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Settings.Enabled";
static constexpr char kPrivacyHubCameraEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Notification.Enabled";

// Report microphone mute events from system and notifications.
ASH_EXPORT void LogMicrophoneEnabledFromSettings(bool enabled);
ASH_EXPORT void LogMicrophoneEnabledFromNotification(bool enabled);

// Report camera mute events from system and notifications.
ASH_EXPORT void LogCameraEnabledFromSettings(bool enabled);
ASH_EXPORT void LogCameraEnabledFromNotification(bool enabled);

}  // namespace ash::privacy_hub_metrics

#endif  // ASH_SYSTEM_PRIVACY_HUB_PRIVACY_HUB_METRICS_H_
