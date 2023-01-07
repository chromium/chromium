// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"

#include <memory>

#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace quick_unlock {

class FingerprintPowerButtonRaceDetectorTest : public testing::Test {
 public:
  void SetUp() override {
    test_start_time_ = base::TimeTicks::Now();
    chromeos::PowerManagerClient::InitializeFake();
    race_detector_ = std::make_unique<FingerprintPowerButtonRaceDetector>(
        chromeos::FakePowerManagerClient::Get());
  }

  void TearDown() override {
    race_detector_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  void AssertHistogramCounts(int power_button_count,
                             int fingerprint_scan_count,
                             int race_true_count,
                             int race_false_count) {
    histogram_tester_.ExpectUniqueSample(
        race_detector_->power_button_pressed_histogram_name_, true,
        power_button_count);
    histogram_tester_.ExpectUniqueSample(
        race_detector_->fingerprint_scan_histogram_name_, true,
        fingerprint_scan_count);
    histogram_tester_.ExpectBucketCount(
        race_detector_->fingerprint_power_button_race_histogram_name_, true,
        race_true_count);
    histogram_tester_.ExpectBucketCount(
        race_detector_->fingerprint_power_button_race_histogram_name_, false,
        race_false_count);
  }

 protected:
  std::unique_ptr<FingerprintPowerButtonRaceDetector> race_detector_;
  // Use same time as reference throughout the entirety of every test.
  // This is to avoid cases where 2 base::TimeTicks::Now() calls are executed
  // more than a second apart, making tests flaky.
  base::TimeTicks test_start_time_;
  // Small enough so that 2 events fall within the 1 second race detection
  // window
  base::TimeDelta race_trigerring_time_delta_ = base::Milliseconds(500);
  // Large enough so that 2 events fall outside the 1 second race detection
  // window
  base::TimeDelta non_race_trigerring_time_delta_ = base::Milliseconds(1500);

 private:
  base::HistogramTester histogram_tester_;
};

TEST_F(FingerprintPowerButtonRaceDetectorTest,
       ConsecutivePowerButtonEventsRaceNotRecorded) {
  race_detector_->PowerButtonEventReceived(/*down=*/true, test_start_time_);
  race_detector_->PowerButtonEventReceived(
      /*down=*/true, test_start_time_ + race_trigerring_time_delta_);
  AssertHistogramCounts(/*power_button_count=*/2,
                        /*fingerprint_scan_count=*/0,
                        /*race_true_count=*/0,
                        /*race_false_count=*/0);
}

TEST_F(FingerprintPowerButtonRaceDetectorTest,
       ConsecutiveFingerprintEventsRaceNotRecorded) {
  race_detector_->FingerprintScanReceived(test_start_time_);
  race_detector_->FingerprintScanReceived(test_start_time_ +
                                          race_trigerring_time_delta_);
  AssertHistogramCounts(/*power_button_count=*/0,
                        /*fingerprint_scan_count=*/2,
                        /*race_true_count=*/0,
                        /*race_false_count=*/0);
}

TEST_F(FingerprintPowerButtonRaceDetectorTest,
       DifferentTypeEventsWithinRaceTimeWindowRaceRecorded) {
  race_detector_->FingerprintScanReceived(test_start_time_);
  race_detector_->PowerButtonEventReceived(
      /*down=*/true, test_start_time_ + race_trigerring_time_delta_);
  AssertHistogramCounts(/*power_button_count=*/1,
                        /*fingerprint_scan_count=*/1,
                        /*race_true_count=*/1,
                        /*race_false_count=*/0);
}

// Same as
// FingerprintPowerButtonRaceDetectorTest.TimeDeltaLessThanOneSecondRaceLogged
// but with power button press before fingerprint scan
TEST_F(FingerprintPowerButtonRaceDetectorTest,
       DifferentTypeEventsWithinRaceTimeWindowRaceRecordedReversed) {
  race_detector_->PowerButtonEventReceived(/*down=*/true, test_start_time_);
  race_detector_->FingerprintScanReceived(test_start_time_ +
                                          race_trigerring_time_delta_);
  AssertHistogramCounts(/*power_button_count=*/1,
                        /*fingerprint_scan_count=*/1,
                        /*race_true_count=*/1,
                        /*race_false_count=*/0);
}

TEST_F(FingerprintPowerButtonRaceDetectorTest,
       DifferentTypeEventsOutsideRaceTimeWindowRaceNotRecorded) {
  race_detector_->FingerprintScanReceived(test_start_time_);
  race_detector_->PowerButtonEventReceived(
      /*down=*/true, test_start_time_ + non_race_trigerring_time_delta_);
  AssertHistogramCounts(/*power_button_count=*/1,
                        /*fingerprint_scan_count=*/1,
                        /*race_true_count=*/0,
                        /*race_false_count=*/1);
}

}  // namespace quick_unlock
}  // namespace ash
