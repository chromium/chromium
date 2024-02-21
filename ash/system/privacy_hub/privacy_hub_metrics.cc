// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_metrics.h"

#include "ash/system/privacy_hub/privacy_hub_controller.h"

#include "base/metrics/histogram_functions.h"

namespace ash::privacy_hub_metrics {

void LogSensorEnabledFromNotification(Sensor sensor, bool enabled) {
  const char* histogram = nullptr;
  switch (sensor) {
    // Location needs to be handled separately as it has 3 states.
    case Sensor::kLocation: {
      histogram = kPrivacyHubGeolocationAccessLevelChangedFromNotification;

      // Only collect events that trigger system geolocation state changes. When
      // `enabled==false` it means the user just dismissed the notification, so
      // no change has happened.
      if (enabled) {
        const auto* controller = ash::GeolocationPrivacySwitchController::Get();
        CHECK(controller);
        base::UmaHistogramEnumeration(histogram, controller->AccessLevel());
      }
      return;
    }
    case Sensor::kCamera: {
      histogram = kPrivacyHubCameraEnabledFromNotificationHistogram;
      break;
    }
    case Sensor::kMicrophone: {
      histogram = kPrivacyHubMicrophoneEnabledFromNotificationHistogram;
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
