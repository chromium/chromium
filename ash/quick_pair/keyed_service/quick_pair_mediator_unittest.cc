// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/companion_app/mock_companion_app_broker.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/feature_status_tracker/fake_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/mock_quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/feature_status_tracker/quick_pair_feature_status_tracker.h"
#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"
#include "ash/quick_pair/message_stream/fake_message_stream_lookup.h"
#include "ash/quick_pair/message_stream/message_stream_lookup.h"
#include "ash/quick_pair/pairing/fake_retroactive_pairing_detector.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/repository/mock_fast_pair_repository.h"
#include "ash/quick_pair/scanning/mock_scanner_broker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/mock_ui_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ash/services/bluetooth_config/adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_adapter_state_controller.h"
#include "chromeos/ash/services/bluetooth_config/fake_discovery_session_manager.h"
#include "chromeos/ash/services/bluetooth_config/in_process_instance.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::Return;

constexpr char kTestMetadataId[] = "718C17";
constexpr char kTestMetadataId2[] = "FF1B63";
constexpr char kTestAddress[] = "test_address";

constexpr base::TimeDelta kDismissedDiscoveryNotificationBanTime =
    base::Seconds(2);
constexpr base::TimeDelta kShortBanDiscoveryNotificationBanTime =
    base::Minutes(5);

// Represents an arbitrary length ban time to demonstrate that the long ban
// is until the state is reset.
constexpr base::TimeDelta kLongBanDiscoveryNotificationBanTime =
    base::Minutes(15);

const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};

}  // namespace

namespace ash {
namespace quick_pair {

class MediatorTest : public AshTestBase {
 public:
  MediatorTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    set_create_quick_pair_mediator(false);

    AshTestBase::SetUp();

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*adapter_, IsPresent()).WillByDefault(testing::Return(true));
    ON_CALL(*adapter_, GetLowEnergyScanSessionHardwareOffloadingStatus())
        .WillByDefault(testing::Return(
            device::BluetoothAdapter::
                LowEnergyScanSessionHardwareOffloadingStatus::kSupported));
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

    ON_CALL(*mock_pairer_broker_, PairDevice)
        .WillByDefault([this](scoped_refptr<Device> device) {
          // Subsequent Pair protocol never attempts to write the account key to
          // the device: |FastPairPairerImpl::AttemptSendAccountKey()|.
          //
          // V1 devices are paired via the Bluetooth Pairing Dialog and no
          // account key is written to the device:
          // |FastPairPairerImpl::FastPairPairerImpl(...)|.
          if (device->protocol() != Protocol::kFastPairSubsequent &&
              device->version() != DeviceFastPairVersion::kV1) {
            mock_pairer_broker_->NotifyAccountKeyWrite(device, std::nullopt);
          }
        });

    std::unique_ptr<UIBroker> ui_broker = std::make_unique<MockUIBroker>();
    mock_ui_broker_ = static_cast<MockUIBroker*>(ui_broker.get());

    std::unique_ptr<CompanionAppBroker> companion_app_broker =
        std::make_unique<MockCompanionAppBroker>();
    mock_companion_app_broker_ =
        static_cast<MockCompanionAppBroker*>(companion_app_broker.get());

    std::unique_ptr<FastPairRepository> fast_pair_repository =
        std::make_unique<MockFastPairRepository>();
    mock_fast_pair_repository_ =
        static_cast<MockFastPairRepository*>(fast_pair_repository.get());

    FastPairHandshakeLookup::UseFakeInstance();
    mediator_ = std::make_unique<Mediator>(
        std::move(tracker), std::move(scanner_broker),
        std::move(retroactive_pairing_detector),
        std::make_unique<FakeMessageStreamLookup>(), std::move(pairer_broker),
        std::move(ui_broker), std::move(companion_app_broker),
        std::move(fast_pair_repository),
        std::make_unique<QuickPairProcessManagerImpl>());

    initial_device_ = base::MakeRefCounted<Device>(
        kTestMetadataId, kTestAddress, Protocol::kFastPairInitial);
    initial_device2_ = base::MakeRefCounted<Device>(
        kTestMetadataId2, kTestAddress, Protocol::kFastPairInitial);
    subsequent_device_ = base::MakeRefCounted<Device>(
        kTestMetadataId, kTestAddress, Protocol::kFastPairSubsequent);
    retroactive_device_ = base::MakeRefCounted<Device>(
        kTestMetadataId, kTestAddress, Protocol::kFastPairRetroactive);
    base::RunLoop().RunUntilIdle();
  }

  void SetHasAtLeastOneDiscoverySessionChanged(
      bool has_at_least_one_discovery_session) {
    fake_discovery_session_manager()->SetIsDiscoverySessionActive(
        /*has_at_least_one_discovery_session = */
        has_at_least_one_discovery_session);
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<FastPairHandshake> CreateHandshake(
      scoped_refptr<Device> device,
      FastPairHandshake::OnCompleteCallback callback) {
    auto fake = std::make_unique<FakeFastPairHandshake>(
        adapter_, std::move(device), std::move(callback));
    return fake;
  }

 protected:
  bluetooth_config::FakeDiscoverySessionManager*
  fake_discovery_session_manager() {
    return ash_test_helper()
        ->bluetooth_config_test_helper()
        ->fake_discovery_session_manager();
  }

  scoped_refptr<Device> initial_device_;
  scoped_refptr<Device> initial_device2_;
  scoped_refptr<Device> subsequent_device_;
  scoped_refptr<Device> retroactive_device_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  raw_ptr<FakeFeatureStatusTracker, DanglingUntriaged> feature_status_tracker_;
  raw_ptr<MockScannerBroker, DanglingUntriaged> mock_scanner_broker_;
  raw_ptr<FakeRetroactivePairingDetector, DanglingUntriaged>
      fake_retroactive_pairing_detector_;
  raw_ptr<MockPairerBroker, DanglingUntriaged> mock_pairer_broker_;
  raw_ptr<MockUIBroker, DanglingUntriaged> mock_ui_broker_;
  raw_ptr<MockCompanionAppBroker, DanglingUntriaged> mock_companion_app_broker_;
  raw_ptr<MockFastPairRepository, DanglingUntriaged> mock_fast_pair_repository_;
  bluetooth_config::FakeAdapterStateController fake_adapter_state_controller_;
  std::unique_ptr<Mediator> mediator_;
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

  // When one or more discovery sessions are active, don't toggle
  // scanning when fast pair enabled changes.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
}

TEST_F(MediatorTest, TogglesScanningWhenHasAtLeastOneDiscoverySessionChanges) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);

  // When fast pair is disabled, don't toggle scanning when "we have at
  // least one discovery session" changes.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
}

TEST_F(MediatorTest,
       CancelsPairingsWhenHasAtLeastOneDiscoverySessionChangesNotPairing) {
  // Start with fast pair enabled and one handshake in progress.
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, initial_device_,
                                                 base::DoNothing());
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // When one or more discovery sessions are active, stop scanning and dismiss
  // notifications. If we aren't actively pairing, dismiss all handshakes.
  EXPECT_CALL(*mock_pairer_broker_, IsPairing).WillOnce(Return(false));
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // When no discovery sessions are active, resume scanning.
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
}

TEST_F(MediatorTest,
       CancelsPairingsWhenHasAtLeastOneDiscoverySessionChangesIsPairing) {
  // Start with fast pair enabled and one handshake in progress.
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, initial_device_,
                                                 base::DoNothing());
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // When one or more discovery sessions are active, stop scanning and dismiss
  // notifications. Simulate the case where the user has already begun pairing
  // before opening Settings, or has initiated V1 device pair.
  EXPECT_CALL(*mock_pairer_broker_, IsPairing).WillOnce(Return(true));
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing).Times(0);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  SetHasAtLeastOneDiscoverySessionChanged(true);
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // When no discovery sessions are active, resume scanning.
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  SetHasAtLeastOneDiscoverySessionChanged(false);
}

TEST_F(MediatorTest, CancelsPairingsWhenFastPairDisabled) {
  // Start with fast pair enabled and one handshake in progress.
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, initial_device_,
                                                 base::DoNothing());
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // When Fast Pair becomes disabled, stop scanning, dismiss
  // notifications, and dismiss all handshakes.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // When Fast Pair becomes enabled, resume scanning.
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
}

TEST_F(MediatorTest, InvokesShowDiscoveryWhenDeviceFound) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
}

TEST_F(MediatorTest,
       InvokesShowDiscovery_OnlyOneinitial_Device_DeviceFound_DifferentDevice) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  EXPECT_CALL(*mock_ui_broker_, ExtendNotification).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  mock_scanner_broker_->NotifyDeviceFound(initial_device2_);
}

TEST_F(MediatorTest,
       InvokesShowDiscovery_OnlyOneinitial_Device_DeviceFound_SameDevice) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  EXPECT_CALL(*mock_ui_broker_, ExtendNotification).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
}

TEST_F(MediatorTest, InvokesShowPairing_V1) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  auto device = base::MakeRefCounted<Device>(kTestMetadataId, kTestAddress,
                                             Protocol::kFastPairInitial);
  initial_device_->set_version(DeviceFastPairVersion::kV1);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kPairToDevice);
}

TEST_F(MediatorTest, DoesNotInvokeShowPairing_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByUser);
}

TEST_F(MediatorTest, DoesNotInvokeShowPairing_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByOs);
}

TEST_F(MediatorTest, DoesNotInvokeShowPairing_LearnMore) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairing).Times(0);
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kLearnMore);
}

TEST_F(MediatorTest, NotifyPairFailure_KeyBasedPairingCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kKeyBasedPairingCharacteristicDiscovery);
}

TEST_F(MediatorTest, NotifyPairFailure_CreateGattConnection) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kCreateGattConnection);
}

TEST_F(MediatorTest, NotifyPairFailure_GattServiceDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kGattServiceDiscovery);
}

TEST_F(MediatorTest, NotifyPairFailure_GattServiceDiscoveryTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kGattServiceDiscoveryTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_DataEncryptorRetrieval) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kDataEncryptorRetrieval);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kPasskeyCharacteristicDiscovery);
}

TEST_F(MediatorTest, NotifyPairFailure_AccountKeyCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kAccountKeyCharacteristicDiscovery);
}

TEST_F(MediatorTest,
       NotifyPairFailure_KeyBasedPairingCharacteristicNotifySession) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_,
      PairFailure::kKeyBasedPairingCharacteristicNotifySession);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyCharacteristicNotifySession) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kPasskeyCharacteristicNotifySession);
}

TEST_F(MediatorTest,
       NotifyPairFailure_KeyBasedPairingCharacteristicNotifySessionTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_,
      PairFailure::kKeyBasedPairingCharacteristicNotifySessionTimeout);
}

TEST_F(MediatorTest,
       NotifyPairFailure_PasskeyCharacteristicNotifySessionTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kPasskeyCharacteristicNotifySessionTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_KeyBasedPairingCharacteristicWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kKeyBasedPairingCharacteristicWrite);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyPairingCharacteristicWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kPasskeyPairingCharacteristicWrite);
}

TEST_F(MediatorTest, NotifyPairFailure_KeyBasedPairingResponseTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyResponseTimeout) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kPasskeyResponseTimeout);
}

TEST_F(MediatorTest, NotifyPairFailure_KeybasedPairingResponseDecryptFailure) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kKeybasedPairingResponseDecryptFailure);
}

TEST_F(MediatorTest, NotifyPairFailure_IncorrectKeyBasedPairingResponseType) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kIncorrectKeyBasedPairingResponseType);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyDecryptFailure) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kPasskeyDecryptFailure);
}

TEST_F(MediatorTest, NotifyPairFailure_IncorrectPasskeyResponseType) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(
      initial_device_, PairFailure::kIncorrectPasskeyResponseType);
}

TEST_F(MediatorTest, NotifyPairFailure_PasskeyMismatch) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kPasskeyMismatch);
}

TEST_F(MediatorTest, NotifyPairFailure_PairingDeviceLost) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kPairingDeviceLost);
}

TEST_F(MediatorTest, NotifyPairFailure_PairingConnect) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kPairingConnect);
}

TEST_F(MediatorTest, NotifyPairFailure_AddressConnect) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed);
  mock_pairer_broker_->NotifyPairFailure(initial_device_,
                                         PairFailure::kAddressConnect);
}

TEST_F(MediatorTest, InvokesShowAssociateAccount) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice);
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount);
  retroactive_device_->set_account_key(kAccountKey1);
  fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(
      retroactive_device_);
  ASSERT_TRUE(retroactive_device_->version().value() ==
              DeviceFastPairVersion::kHigherThanV1);
}

TEST_F(MediatorTest,
       InvokesShowDiscovery_OnlyOneNotification_DifferentDeviceProtocols) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);
}

TEST_F(MediatorTest, InvokesShowDiscovery_OnlyOneNotification_DifferentDevice) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  mock_scanner_broker_->NotifyDeviceFound(initial_device2_);
}

TEST_F(MediatorTest, InvokesShowDiscovery_OnlyOneNotification_SameDevice) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
}

TEST_F(MediatorTest, DoesntInvokeShowAssociateAccount_FastPairDisabled) {
  feature_status_tracker_->SetIsFastPairEnabled(false);
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount).Times(0);
  fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(
      initial_device_);
}

TEST_F(
    MediatorTest,
    NoNotificationOnAccountKeyWriteFailure_AccountKeyCharacteristicDiscovery) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed).Times(0);
  mock_pairer_broker_->NotifyAccountKeyWrite(
      initial_device_, AccountKeyFailure::kAccountKeyCharacteristicDiscovery);
}

TEST_F(MediatorTest,
       NoNotificationOnAccountKeyWriteFailure_kAccountKeyCharacteristicWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPairingFailed).Times(0);
  mock_pairer_broker_->NotifyAccountKeyWrite(
      initial_device_, AccountKeyFailure::kAccountKeyCharacteristicDiscovery);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_AssociateAccount) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_fast_pair_repository_, WriteAccountAssociationToFootprints);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  retroactive_device_->set_version(DeviceFastPairVersion::kHigherThanV1);
  retroactive_device_->set_account_key(kAccountKey1);
  mock_ui_broker_->NotifyAssociateAccountAction(
      retroactive_device_, AssociateAccountAction::kAssociateAccount);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_LearnMore) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications).Times(0);
  mock_ui_broker_->NotifyAssociateAccountAction(
      initial_device_, AssociateAccountAction::kLearnMore);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyAssociateAccountAction(
      initial_device_, AssociateAccountAction::kDismissedByUser);
}

TEST_F(MediatorTest, AssociateAccountKeyAction_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyAssociateAccountAction(
      initial_device_, AssociateAccountAction::kDismissedByUser);
}

TEST_F(MediatorTest, CompanionAppAction_DownloadApp_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_DEATH_IF_SUPPORTED(
      {
        mock_ui_broker_->NotifyCompanionAppAction(
            initial_device_, CompanionAppAction::kDownloadAndLaunchApp);
      },
      "");
}

TEST_F(MediatorTest, CompanionAppAction_DownloadApp_Enabled) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_companion_app_broker_, InstallCompanionApp).Times(1);
  mock_ui_broker_->NotifyCompanionAppAction(
      initial_device_, CompanionAppAction::kDownloadAndLaunchApp);
}

TEST_F(MediatorTest, CompanionAppAction_LaunchApp_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_DEATH_IF_SUPPORTED(
      {
        mock_ui_broker_->NotifyCompanionAppAction(
            initial_device_, CompanionAppAction::kLaunchApp);
      },
      "");
}

TEST_F(MediatorTest, CompanionAppAction_LaunchApp_Enabled) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_companion_app_broker_, LaunchCompanionApp).Times(1);
  mock_ui_broker_->NotifyCompanionAppAction(initial_device_,
                                            CompanionAppAction::kLaunchApp);
}

TEST_F(MediatorTest, CompanionAppAction_DismissedByUser_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_DEATH_IF_SUPPORTED(
      {
        mock_ui_broker_->NotifyCompanionAppAction(
            initial_device_, CompanionAppAction::kDismissedByUser);
      },
      "");
}

TEST_F(MediatorTest, CompanionAppAction_DismissedByUser_Enabled) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyCompanionAppAction(
      initial_device_, CompanionAppAction::kDismissedByUser);
}

TEST_F(MediatorTest, CompanionAppAction_Dismissed_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_DEATH_IF_SUPPORTED(
      {
        mock_ui_broker_->NotifyCompanionAppAction(
            initial_device_, CompanionAppAction::kDismissed);
      },
      "");
}

TEST_F(MediatorTest, CompanionAppAction_Dismissed_Enabled) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyCompanionAppAction(initial_device_,
                                            CompanionAppAction::kDismissed);
}

TEST_F(MediatorTest, PairingFailedAction_NavigateToSettings) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyPairingFailedAction(
      initial_device_, PairingFailedAction::kNavigateToSettings);
}

TEST_F(MediatorTest, PairingFailedAction_DismissedByUser) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyPairingFailedAction(
      initial_device_, PairingFailedAction::kDismissedByUser);
}

TEST_F(MediatorTest, PairingFailedAction_Dismissed) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_pairer_broker_, PairDevice).Times(0);
  mock_ui_broker_->NotifyPairingFailedAction(initial_device_,
                                             PairingFailedAction::kDismissed);
}

TEST_F(MediatorTest, FastPairBluetoothConfigDelegate) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  bluetooth_config::FastPairDelegate* delegate =
      mediator_->GetFastPairDelegate();
  delegate->SetDeviceNameManager(nullptr);
  delegate->SetAdapterStateController(nullptr);
  EXPECT_TRUE(delegate);
  EXPECT_EQ(delegate->GetDeviceImageInfo(kTestAddress), std::nullopt);
}

TEST_F(MediatorTest,
       FastPairBluetoothConfigDelegateNotifiesAdapterStateChanges) {
  EXPECT_CALL(*mock_scanner_broker_, StartScanning);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  FastPairHandshakeLookup::GetInstance()->Create(adapter_, initial_device_,
                                                 base::DoNothing());

  bluetooth_config::FastPairDelegate* delegate =
      mediator_->GetFastPairDelegate();
  delegate->SetDeviceNameManager(nullptr);

  // Mediator should not observe changes to the adapter state before the
  // AdapterStateController is set and the observation is created.
  fake_adapter_state_controller_.SetSystemState(
      bluetooth_config::mojom::BluetoothSystemState::kDisabling);
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));
  fake_adapter_state_controller_.SetSystemState(
      bluetooth_config::mojom::BluetoothSystemState::kEnabled);
  EXPECT_TRUE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  // After the AdapterStateController is set, we should be notified and
  // create an observation.
  delegate->SetAdapterStateController(&fake_adapter_state_controller_);

  // Simulate a call to toggling Bluetooth off via the UI. This should stop
  // scanning, clear existing handshakes, stop pairing, and dismiss
  // notifications.
  EXPECT_CALL(*mock_scanner_broker_, StopScanning);
  EXPECT_CALL(*mock_pairer_broker_, StopPairing);
  EXPECT_CALL(*mock_ui_broker_, RemoveNotifications);
  fake_adapter_state_controller_.SetSystemState(
      bluetooth_config::mojom::BluetoothSystemState::kDisabling);
  EXPECT_FALSE(FastPairHandshakeLookup::GetInstance()->Get(initial_device_));

  delegate->SetAdapterStateController(nullptr);
}

TEST_F(MediatorTest, DiscoveryBanLogic_InitialParing) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // Simulate the device first found. When the user dismissed the notification,
  // if the device is found again, we do not expect the notification to be
  // shown.
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // After the 2 second timeout, when the device is found again, expect
  // the notification to be shown.
  task_environment()->FastForwardBy(kDismissedDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // When the device is found again, we expect the notification to not be shown
  // since it's within the 5 minute short ban timeout period after it has been
  // dismissed by user again.
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // After the 5 minute timeout, when the device is found again, expect
  // the notification to be shown.
  task_environment()->FastForwardBy(kShortBanDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // When the device is found again, we expect the notification to not be shown
  // again since it's in the long ban state.
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // Even after a long timeout period, we do not expect the notification to be
  // shown again under the long ban period.
  task_environment()->FastForwardBy(kLongBanDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // We expect the notification to be shown again when the Fast Pair
  // state is reset. Simulate the Fast Pair toggle being turned off then on
  // again.
  feature_status_tracker_->SetIsFastPairEnabled(false);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // We also expect the notification to be shown again after a successful
  // pairing. Trigger the ban logic.
  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
  // Trigger a successful pairing
  mock_pairer_broker_->NotifyDevicePaired(initial_device_);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
}

TEST_F(MediatorTest, DiscoveryBan_SubsequentParing) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // Simulate the device first found. When the user dismissed the notification,
  // if the device is found again, we do not expect the notification to be
  // shown.
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);
  mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // After the 2 second timeout, when the device is found again, expect
  // the notification to be shown.
  task_environment()->FastForwardBy(kDismissedDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // When the device is found again, we expect the notification to not be shown
  // since it's within the 5 minute short ban timeout period after it has been
  // dismissed by user again.
  mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // After the 5 minute timeout, when the device is found again, expect
  // the notification to be shown.
  task_environment()->FastForwardBy(kShortBanDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // When the device is found again, we expect the notification to not be shown
  // again since it's in the long ban state.
  mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // Even after a long timeout period, we do not expect the notification to be
  // shown again under the long ban period.
  task_environment()->FastForwardBy(kLongBanDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // We expect the notification to be shown again when the Fast Pair
  // state is reset. Simulate the Fast Pair toggle being turned off then on
  // again.
  feature_status_tracker_->SetIsFastPairEnabled(false);
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);

  // We also expect the notification to be shown again after a successful
  // pairing. Trigger the ban logic.
  mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);
  // Trigger a successful pairing
  mock_pairer_broker_->NotifyDevicePaired(subsequent_device_);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(subsequent_device_);
}

TEST_F(MediatorTest, DiscoveryBan_MultipleDevices) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // Simulate the device first found. When the user dismissed the notification,
  // if the device is found again, we do not expect the notification to be
  // shown.
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // However, at this point, a different device is able to be shown
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device2_);

  // After the 2 second timeout, when the device is found again, don't expect
  // the notification to be shown since the second device is currently
  // being shown.
  task_environment()->FastForwardBy(kDismissedDiscoveryNotificationBanTime);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // When the second device is dismissed, do not expect it be to shown
  // again since it is blocked.
  mock_ui_broker_->NotifyDiscoveryAction(initial_device2_,
                                         DiscoveryAction::kDismissedByUser);
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device2_);

  // Now the first device can be shown again since first come first serve
  // notifications no longer apply and it is no longer blocked.
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);
}

TEST_F(MediatorTest, DiscoveryBan_RetroactiveAvoidsBan) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // Simulate the device first found.
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(1);
  mock_scanner_broker_->NotifyDeviceFound(initial_device_);

  // Simulate another device being found. We expect no notification to be
  // shown for this device due to our existing first come first serve
  // notification logic.
  EXPECT_CALL(*mock_ui_broker_, ShowDiscovery).Times(0);
  mock_scanner_broker_->NotifyDeviceFound(initial_device2_);

  // However, there is an exception in the first come first serve notification
  // logic to show retroactive devices.
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount).Times(1);
  fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(
      retroactive_device_);
}

TEST_F(MediatorTest, PersistsDeviceImages_AfterRetroactivePairFound) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // We should save mac address to model ID mapping and persist images
  // once Retroactive Pair is found--in other words, a device was just
  // classic paired and we have images for that device we want to
  // display in Bluetooth Settings, even if the user is offline/logged
  // out/etc.
  EXPECT_CALL(*mock_fast_pair_repository_, FetchDeviceImages).Times(1);
  EXPECT_CALL(*mock_fast_pair_repository_, PersistDeviceImages).Times(1);
  fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(
      retroactive_device_);
}

TEST_F(MediatorTest, PersistsDeviceImages_AfterDeviceInitialPaired) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // We should save mac address to model ID mapping and persist images
  // once a device is paired. We have images for the paired device we want to
  // display in Bluetooth Settings, even if the user is offline/logged
  // out/etc.
  EXPECT_CALL(*mock_fast_pair_repository_, FetchDeviceImages).Times(1);
  EXPECT_CALL(*mock_fast_pair_repository_, PersistDeviceImages).Times(1);
  mock_pairer_broker_->NotifyDevicePaired(initial_device_);
}

TEST_F(MediatorTest, PersistsDeviceImages_AfterDeviceSubsequentPaired) {
  feature_status_tracker_->SetIsFastPairEnabled(true);

  // We should save mac address to model ID mapping and persist images
  // once a device is paired. We have images for the paired device we want to
  // display in Bluetooth Settings, even if the user is offline/logged
  // out/etc.
  EXPECT_CALL(*mock_fast_pair_repository_, FetchDeviceImages).Times(1);
  EXPECT_CALL(*mock_fast_pair_repository_, PersistDeviceImages).Times(1);
  mock_pairer_broker_->NotifyDevicePaired(subsequent_device_);
}

TEST_F(MediatorTest,
       ShowAssociateAccount_OnRetroactivePairSilentAccountKeyWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  retroactive_device_->set_account_key(kAccountKey1);
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount);
  mock_pairer_broker_->NotifyAccountKeyWrite(retroactive_device_,
                                             /*error=*/std::nullopt);
}

TEST_F(MediatorTest, NoShowAssociateAccount_OnInitialPairAccountKeyWrite) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  initial_device_->set_account_key(kAccountKey1);
  EXPECT_CALL(*mock_ui_broker_, ShowAssociateAccount).Times(0);
  mock_pairer_broker_->NotifyAccountKeyWrite(initial_device_,
                                             /*error=*/std::nullopt);
}

TEST_F(MediatorTest, ShowCompanionApp_OnDevicePaired_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_companion_app_broker_, MaybeShowCompanionAppActions)
      .Times(0);
  mock_pairer_broker_->NotifyDevicePaired(initial_device_);
}

TEST_F(MediatorTest, ShowCompanionApp_OnDevicePaired_Enabled) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairPwaCompanion};

  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_companion_app_broker_, MaybeShowCompanionAppActions)
      .Times(1);
  mock_pairer_broker_->NotifyDevicePaired(initial_device_);
}

TEST_F(MediatorTest, ShowPasskey_OnDisplayPasskey) {
  feature_status_tracker_->SetIsFastPairEnabled(true);
  EXPECT_CALL(*mock_ui_broker_, ShowPasskey);
  mock_pairer_broker_->NotifyDisplayPasskey(/*device name=*/std::u16string(),
                                            /*passkey=*/0);
}

}  // namespace quick_pair
}  // namespace ash
