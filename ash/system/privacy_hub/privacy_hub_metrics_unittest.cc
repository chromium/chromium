// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_metrics.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::privacy_hub_metrics {
namespace {

const auto kGeolocationAccessLevels = {
    GeolocationAccessLevel::kDisallowed,
    GeolocationAccessLevel::kAllowed,
    GeolocationAccessLevel::kOnlyAllowedForSystem,
};

}  // namespace

using Sensor = SensorDisabledNotificationDelegate::Sensor;

class PrivacyHubMetricsTest : public AshTestBase {
 public:
  PrivacyHubMetricsTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(PrivacyHubMetricsTest, EnableFromNotification) {
  const base::HistogramTester histogram_tester;

  // Test Microphone and Camera:
  for (const bool enabled : {true, false}) {
    histogram_tester.ExpectBucketCount(
        kPrivacyHubCameraEnabledFromNotificationHistogram, enabled, 0);
    LogSensorEnabledFromNotification(Sensor::kCamera, enabled);
    histogram_tester.ExpectBucketCount(
        kPrivacyHubCameraEnabledFromNotificationHistogram, enabled, 1);

    histogram_tester.ExpectBucketCount(
        kPrivacyHubMicrophoneEnabledFromNotificationHistogram, enabled, 0);
    LogSensorEnabledFromNotification(Sensor::kMicrophone, enabled);
    histogram_tester.ExpectBucketCount(
        kPrivacyHubMicrophoneEnabledFromNotificationHistogram, enabled, 1);
  }

  // Test Location:
  // Notification dismissal is not recorded for location access level change.
  LogSensorEnabledFromNotification(Sensor::kLocation, false);
  for (auto access_level : kGeolocationAccessLevels) {
    histogram_tester.ExpectBucketCount(
        kPrivacyHubGeolocationAccessLevelChangedFromNotification, access_level,
        0);
  }
  LogSensorEnabledFromNotification(Sensor::kLocation, true);
  histogram_tester.ExpectBucketCount(
      kPrivacyHubGeolocationAccessLevelChangedFromNotification,
      GeolocationAccessLevel::kAllowed, 1);
}

TEST_F(PrivacyHubMetricsTest, OpenFromNotification) {
  const base::HistogramTester histogram_tester;

  histogram_tester.ExpectBucketCount(
      kPrivacyHubOpenedHistogram, PrivacyHubNavigationOrigin::kNotification, 0);
  LogPrivacyHubOpenedFromNotification();
  histogram_tester.ExpectBucketCount(
      kPrivacyHubOpenedHistogram, PrivacyHubNavigationOrigin::kNotification, 1);
}

}  // namespace ash::privacy_hub_metrics
