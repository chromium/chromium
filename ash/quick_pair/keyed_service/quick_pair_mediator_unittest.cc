// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/feature_status_tracker/fake_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/mock_quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/message_stream/fake_message_stream_lookup.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/pairing/fake_retroactive_pairing_detector.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/scanning/mock_scanner_broker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/mock_ui_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kTestMetadataId[] = "test_metadata_id";
constexpr char kTestAddress[] = "test_address";

}  // namespace

namespace ash {
namespace quick_pair {

class MediatorTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    std::unique_ptr<FeatureStatusTracker> tracker =
        std::make_unique<FakeFeatureStatusTracker>();
    feature_status_tracker_ =
        static_cast<FakeFeatureStatusTracker*>(tracker.get());

    std::unique_ptr<ScannerBroker> scanner_broker =
        std::make_unique<MockScannerBroker>();
    mock_scanner_broker_ =
        static_cast<MockScannerBroker*>(scanner_broker.get());

    std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector =
        std::make_unique<FakeRetroactivePairingDetector>();
    fake_retroactive_pairing_detector_ =
        static_cast<FakeRetroactivePairingDetector*>(
            retroactive_pairing_detector.get());

    std::unique_ptr<PairerBroker> pairer_broker =
        std::make_unique<MockPairerBroker>();
    mock_pairer_broker_ = static_cast<MockPairerBroker*>(pairer_broker.get());

    std::unique_ptr<UIBroker> ui_broker = std::make_unique<MockUIBroker>();
    mock_ui_broker_ = static_cast<MockUIBroker*>(ui_broker.get());

    mediator_ = std::make_unique<Mediator>(
        std::move(tracker), std::move(scanner_broker),
        std::move(retroactive_pairing_detector),
        std::make_unique<FakeMessageStreamLookup>(), std::move(pairer_broker),
        std::move(ui_broker), std::unique_ptr<FastPairRepository>(),
        std::make_unique<QuickPairProcessManagerImpl>());

    device_ = base::MakeRefCounted<Device>(kTestMetadataId, kTestAddress,
                                           Protocol::kFastPairInitial);
  }

 protected:
  scoped_refptr<Device> device_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  FakeFeatureStatusTracker* feature_status_tracker_;
  MockScannerBroker* mock_scanner_broker_;
  FakeRetroactivePairingDetector* fake_retroactive_pairing_detector_;
  MockPairerBroker* mock_pairer_broker_;
  MockUIBroker* mock_ui_broker_;
  std::unique_ptr<Mediator> mediator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(MediatorTest, TogglesScanningWhenFastPairEnabledChanges) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
}

TEST_F(MediatorTest, InvokesShowDiscoveryWhenDeviceFound) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery);
  mock_scanner_broker_->NotifyDeviceFound(device_);
}

TEST_F(MediatorTest, InvokesShowPairingOnAppropriateAction) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing);
  mock_ui_broker_->NotifyDiscoveryAction(device_,
                                         DiscoveryAction::kPairToDevice);
}

TEST_F(MediatorTest, NotifyPairFailure) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      device_, PairFailure::kKeyBasedPairingCharacteristicDiscovery);
}

TEST_F(MediatorTest, InvokesShowAssociateAccount) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount);
  fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(device_);
}

TEST_F(MediatorTest, RemoveNotificationOnPaired) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  mock_pairer_broker_->NotifyDevicePaired(device_);
}

TEST_F(MediatorTest, RemoveNotificationOnDeviceLost) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  mock_scanner_broker_->NotifyDeviceLost(device_);
}

}  // namespace quick_pair
}  // namespace ash
