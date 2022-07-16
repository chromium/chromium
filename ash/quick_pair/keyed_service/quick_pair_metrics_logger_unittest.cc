// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/scanning/mock_scanner_broker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/mock_ui_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kTestMetadataId[] = "test_metadata_id";
constexpr char kTestAddress[] = "test_address";
constexpr char kFastPairEngagementFlowMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps";

}  // namespace

namespace ash {
namespace quick_pair {

class QuickPairMetricsLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    scanner_broker_ = std::make_unique<MockScannerBroker>();
    mock_scanner_broker_ =
        static_cast<MockScannerBroker*>(scanner_broker_.get());

    pairer_broker_ = std::make_unique<MockPairerBroker>();
    mock_pairer_broker_ = static_cast<MockPairerBroker*>(pairer_broker_.get());

    ui_broker_ = std::make_unique<MockUIBroker>();
    mock_ui_broker_ = static_cast<MockUIBroker*>(ui_broker_.get());

    device_ = base::MakeRefCounted<Device>(kTestMetadataId, kTestAddress,
                                           Protocol::kFastPairInitial);

    metrics_logger_ = std::make_unique<QuickPairMetricsLogger>(
        scanner_broker_.get(), pairer_broker_.get(), ui_broker_.get());
  }

  void SimulateDiscoveryUiShown() {
    mock_scanner_broker_->NotifyDeviceFound(device_);
  }

  void SimulateDiscoveryUiDismissed() {
    mock_ui_broker_->NotifyDiscoveryAction(device_,
                                           DiscoveryAction::kDismissedByUser);
  }

  void SimulateDiscoveryUiConnectPressed() {
    mock_ui_broker_->NotifyDiscoveryAction(device_,
                                           DiscoveryAction::kPairToDevice);
  }

  void SimulatePairingFailed() {
    mock_pairer_broker_->NotifyPairFailure(
        device_, PairFailure::kKeyBasedPairingCharacteristicDiscovery);
  }

  void SimulatePairingSucceeded() {
    mock_pairer_broker_->NotifyDevicePaired(device_);
  }

  void SimulateErrorUiDismissed() {
    mock_ui_broker_->NotifyPairingFailedAction(
        device_, PairingFailedAction::kDismissedByUser);
  }

  void SimulateErrorUiSettingsPressed() {
    mock_ui_broker_->NotifyPairingFailedAction(
        device_, PairingFailedAction::kNavigateToSettings);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<Device> device_;

  MockScannerBroker* mock_scanner_broker_ = nullptr;
  MockPairerBroker* mock_pairer_broker_ = nullptr;
  MockUIBroker* mock_ui_broker_ = nullptr;

  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  std::unique_ptr<UIBroker> ui_broker_;
  std::unique_ptr<QuickPairMetricsLogger> metrics_logger_;
};

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiShown) {
  SimulateDiscoveryUiShown();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiDismissed) {
  SimulateDiscoveryUiDismissed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiConnectPressed) {
  SimulateDiscoveryUiConnectPressed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairingFailed) {
  SimulatePairingFailed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairingSucceeded) {
  SimulatePairingSucceeded();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiDismissed) {
  SimulateErrorUiDismissed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiSettingsPressed) {
  SimulateErrorUiSettingsPressed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetric,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            1);
}

}  // namespace quick_pair
}  // namespace ash
