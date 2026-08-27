// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/on_device_encryption_metrics_reporter.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/ode/on_device_encryption_state_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class OnDeviceEncryptionMetricsReporterTest : public testing::Test {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(OnDeviceEncryptionMetricsReporterTest, InitialStateRecordedIfAvailable) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  passkey_tracker->SetStateForTesting(OnDeviceEncryptionState::kDeviceReady);

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kOnDeviceEncryptionNotEnabled, 1);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest,
       InitialStateNotAvailableDoesNotRecord) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     0);
}

TEST_F(OnDeviceEncryptionMetricsReporterTest, StateTransitionsRecorded) {
  auto passkey_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_passkey_tracker = passkey_tracker.get();

  auto password_tracker = std::make_unique<OnDeviceEncryptionStateTracker>();
  OnDeviceEncryptionStateTracker* raw_password_tracker = password_tracker.get();

  OnDeviceEncryptionMetricsReporter reporter(std::move(passkey_tracker),
                                             std::move(password_tracker));

  histogram_tester_.ExpectTotalCount(kPasskeyOnDeviceEncryptionStateHistogram,
                                     0);
  histogram_tester_.ExpectTotalCount(kPasswordOnDeviceEncryptionStateHistogram,
                                     0);

  // Transition passkey tracker from NotAvailable -> DeviceNotReady.
  raw_passkey_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceNotReady);
  histogram_tester_.ExpectUniqueSample(
      kPasskeyOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceNotReady, 1);

  // Transition password tracker from NotAvailable -> DeviceReady.
  raw_password_tracker->SetStateForTesting(
      OnDeviceEncryptionState::kDeviceReady);
  histogram_tester_.ExpectUniqueSample(
      kPasswordOnDeviceEncryptionStateHistogram,
      OnDeviceEncryptionStateHistogramBucket::kDeviceReady, 1);
}

}  // namespace

}  // namespace password_manager
