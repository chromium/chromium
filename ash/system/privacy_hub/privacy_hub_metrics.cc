// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash::privacy_hub_metrics {

namespace {

constexpr char kPrivacyHubMicrophoneEnabledFromSettingsHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Settings.Enabled";
constexpr char kPrivacyHubMicrophoneEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Microphone.Notification.Enabled";
constexpr char kPrivacyHubCameraEnabledFromSettingsHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Settings.Enabled";
constexpr char kPrivacyHubCameraEnabledFromNotificationHistogram[] =
    "ChromeOS.PrivacyHub.Camera.Notification.Enabled";

}  // namespace

void LogMicrophoneEnabledFromSettings(bool enabled) {
  base::UmaHistogramBoolean(kPrivacyHubMicrophoneEnabledFromSettingsHistogram,
                            enabled);
}

void LogMicrophoneEnabledFromNotification(bool enabled) {
  base::UmaHistogramBoolean(
      kPrivacyHubMicrophoneEnabledFromNotificationHistogram, enabled);
}

void LogCameraEnabledFromSettings(bool enabled) {
  base::UmaHistogramBoolean(kPrivacyHubCameraEnabledFromSettingsHistogram,
                            enabled);
}

void LogCameraEnabledFromNotification(bool enabled) {
  base::UmaHistogramBoolean(kPrivacyHubCameraEnabledFromNotificationHistogram,
                            enabled);
}

}  // namespace ash::privacy_hub_metrics
