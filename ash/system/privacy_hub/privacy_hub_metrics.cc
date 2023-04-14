// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash::privacy_hub_metrics {

void LogSensorEnabledFromSettings(Sensor sensor, bool enabled) {
  const char* histogram = nullptr;
  switch (sensor) {
    case Sensor::kCamera: {
      histogram = kPrivacyHubCameraEnabledFromSettingsHistogram;
      break;
    }
    case Sensor::kMicrophone: {
      histogram = kPrivacyHubMicrophoneEnabledFromSettingsHistogram;
      break;
    }
    case Sensor::kLocation: {
      histogram = kPrivacyHubGeolocationEnabledFromSettingsHistogram;
      break;
    }
  }
  CHECK(histogram);
  base::UmaHistogramBoolean(histogram, enabled);
}

void LogSensorEnabledFromNotification(Sensor sensor, bool enabled) {
  const char* histogram = nullptr;
  switch (sensor) {
    case Sensor::kCamera: {
      histogram = kPrivacyHubCameraEnabledFromNotificationHistogram;
      break;
    }
    case Sensor::kMicrophone: {
      histogram = kPrivacyHubMicrophoneEnabledFromNotificationHistogram;
      break;
    }
    case Sensor::kLocation: {
      histogram = kPrivacyHubGeolocationEnabledFromNotificationHistogram;
      break;
    }
  }
  CHECK(histogram);
  base::UmaHistogramBoolean(histogram, enabled);
}

void LogPrivacyHubOpenedFromNotification() {
  base::UmaHistogramEnumeration(kPrivacyHubOpenedHistogram,
                                PrivacyHubNavigationOrigin::kNotification);
}

void LogPrivacyHubLearnMorePageOpened(PrivacyHubLearnMoreSensor sensor) {
  base::UmaHistogramEnumeration(kPrivacyHubLearnMorePageOpenedHistogram,
                                sensor);
}

}  // namespace ash::privacy_hub_metrics
