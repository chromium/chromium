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
#include "ash/quick_pair/pairing/fake_retroactive_pairing_detector.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
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
constexpr char kFastPairEngagementFlowMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol";
constexpr char kFastPairEngagementFlowMetricSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps."
    "SubsequentPairingProtocol";
const char kFastPairRetroactiveEngagementFlowMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactiveEngagementFunnel.Steps";
constexpr char kFastPairPairTimeMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.InitialPairingProtocol";
constexpr char kFastPairPairTimeMetricSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.SubsequentPairingProtocol";

}  // namespace

namespace ash {
namespace quick_pair {

class QuickPairMetricsLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    scanner_broker_ = std::make_unique<MockScannerBroker>();
    mock_scanner_broker_ =
        static_cast<MockScannerBroker*>(scanner_broker_.get());

    retroactive_pairing_detector_ =
        std::make_unique<FakeRetroactivePairingDetector>();
    fake_retroactive_pairing_detector_ =
        static_cast<FakeRetroactivePairingDetector*>(
            retroactive_pairing_detector_.get());

    pairer_broker_ = std::make_unique<MockPairerBroker>();
    mock_pairer_broker_ = static_cast<MockPairerBroker*>(pairer_broker_.get());

    ui_broker_ = std::make_unique<MockUIBroker>();
    mock_ui_broker_ = static_cast<MockUIBroker*>(ui_broker_.get());

    initial_device_ = base::MakeRefCounted<Device>(
        kTestMetadataId, kTestAddress, Protocol::kFastPairInitial);
    subsequent_device_ = base::MakeRefCounted<Device>(
        kTestMetadataId, kTestAddress, Protocol::kFastPairSubsequent);
    retroactive_device_ = base::MakeRefCounted<Device>(
        kTestMetadataId, kTestAddress, Protocol::kFastPairRetroactive);

    metrics_logger_ = std::make_unique<QuickPairMetricsLogger>(
        scanner_broker_.get(), pairer_broker_.get(), ui_broker_.get(),
        retroactive_pairing_detector_.get());
  }

  void SimulateDiscoveryUiShown(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_scanner_broker_->NotifyDeviceFound(initial_device_);
        break;
      case Protocol::kFastPairSubsequent:
        mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateDiscoveryUiDismissed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyDiscoveryAction(
            initial_device_, DiscoveryAction::kDismissedByUser);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyDiscoveryAction(
            subsequent_device_, DiscoveryAction::kDismissedByUser);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateDiscoveryUiConnectPressed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                               DiscoveryAction::kPairToDevice);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                               DiscoveryAction::kPairToDevice);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulatePairingFailed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_pairer_broker_->NotifyPairFailure(
            initial_device_,
            PairFailure::kKeyBasedPairingCharacteristicDiscovery);
        break;
      case Protocol::kFastPairSubsequent:
        mock_pairer_broker_->NotifyPairFailure(
            subsequent_device_,
            PairFailure::kKeyBasedPairingCharacteristicDiscovery);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulatePairingSucceeded(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_pairer_broker_->NotifyDevicePaired(initial_device_);
        break;
      case Protocol::kFastPairSubsequent:
        mock_pairer_broker_->NotifyDevicePaired(subsequent_device_);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateErrorUiDismissed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyPairingFailedAction(
            initial_device_, PairingFailedAction::kDismissedByUser);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyPairingFailedAction(
            subsequent_device_, PairingFailedAction::kDismissedByUser);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateErrorUiSettingsPressed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyPairingFailedAction(
            initial_device_, PairingFailedAction::kNavigateToSettings);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyPairingFailedAction(
            subsequent_device_, PairingFailedAction::kNavigateToSettings);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateAssociateAccountUiShown() {
    fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(
        retroactive_device_);
  }

  void SimulateAssociateAccountUiDismissed() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kDismissed);
  }

  void SimulateAssociateAccountUiDismissedByUser() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kDismissedByUser);
  }

  void SimulateAssociateAccountUiSavePressed() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kAssoicateAccount);
  }

  void SimulateAssociateAccountUiLearnMorePressed() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kLearnMore);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<Device> initial_device_;
  scoped_refptr<Device> subsequent_device_;
  scoped_refptr<Device> retroactive_device_;

  MockScannerBroker* mock_scanner_broker_ = nullptr;
  MockPairerBroker* mock_pairer_broker_ = nullptr;
  MockUIBroker* mock_ui_broker_ = nullptr;
  FakeRetroactivePairingDetector* fake_retroactive_pairing_detector_ = nullptr;

  std::unique_ptr<ScannerBroker> scanner_broker_;
  std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  std::unique_ptr<UIBroker> ui_broker_;
  std::unique_ptr<QuickPairMetricsLogger> metrics_logger_;
};

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiShown_Initial) {
  SimulateDiscoveryUiShown(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiShown_Subsequent) {
  SimulateDiscoveryUiShown(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiDismissed_Initial) {
  SimulateDiscoveryUiDismissed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiDismissed_Subsequent) {
  SimulateDiscoveryUiDismissed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiConnectPressed_Initial) {
  SimulateDiscoveryUiConnectPressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiConnectPressed_Subsequent) {
  SimulateDiscoveryUiConnectPressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairingFailed_Initial) {
  SimulatePairingFailed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairingFailed_Subsequent) {
  SimulatePairingFailed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairingSucceeded_Initial) {
  SimulatePairingSucceeded(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairingSucceeded_Subsequent) {
  SimulatePairingSucceeded(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiDismissed_Initial) {
  SimulateErrorUiDismissed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiDismissed_Subsequent) {
  SimulateErrorUiDismissed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiSettingsPressed_Initial) {
  SimulateErrorUiSettingsPressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiSettingsPressed_Subsequent) {
  SimulateErrorUiSettingsPressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiShown),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiConnectPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingFailed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kPairingSucceeded),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairTime_Initial) {
  SimulateDiscoveryUiConnectPressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  histogram_tester().ExpectTotalCount(kFastPairPairTimeMetricInitial, 0);

  SimulatePairingSucceeded(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();
  histogram_tester().ExpectTotalCount(kFastPairPairTimeMetricInitial, 1);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairTime_Subsequent) {
  SimulateDiscoveryUiConnectPressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  histogram_tester().ExpectTotalCount(kFastPairPairTimeMetricSubsequent, 0);

  SimulatePairingSucceeded(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();
  histogram_tester().ExpectTotalCount(kFastPairPairTimeMetricSubsequent, 1);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountShown) {
  SimulateAssociateAccountUiShown();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      1);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountDismissed) {
  SimulateAssociateAccountUiDismissed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountDismissedByUser) {
  SimulateAssociateAccountUiDismissedByUser();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            1);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountSavePressed) {
  SimulateAssociateAccountUiSavePressed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountLearnMorePressed) {
  SimulateAssociateAccountUiLearnMorePressed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogAssociateAccountLearnMorePressed_SavePressed) {
  SimulateAssociateAccountUiLearnMorePressed();
  base::RunLoop().RunUntilIdle();
  SimulateAssociateAccountUiSavePressed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogAssociateAccountLearnMorePressed_Dismissed) {
  SimulateAssociateAccountUiLearnMorePressed();
  base::RunLoop().RunUntilIdle();
  SimulateAssociateAccountUiDismissed();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            1);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogAssociateAccountLearnMorePressed_DismissedByUser) {
  SimulateAssociateAccountUiLearnMorePressed();
  base::RunLoop().RunUntilIdle();
  SimulateAssociateAccountUiDismissedByUser();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiShown),
      0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountUiDismissed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
            0);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetric,
          FastPairRetroactiveEngagementFlowEvent::kAssociateAccountSavePressed),
      0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountSavePressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
}

}  // namespace quick_pair
}  // namespace ash
