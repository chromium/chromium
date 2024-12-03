// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/quick_pair_metrics_logger.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/fake_retroactive_pairing_detector.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/scanning/mock_scanner_broker.h"
#include "ash/quick_pair/scanning/scanner_broker.h"
#include "ash/quick_pair/ui/mock_ui_broker.h"
#include "ash/quick_pair/ui/ui_broker.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAddress[] = "test_address";
constexpr char kFastPairEngagementFlowMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol";
constexpr char kFastPairEngagementFlowMetricSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps."
    "SubsequentPairingProtocol";
constexpr char kFastPairEngagementFlowMetricInitialWithFakeMetadata[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol."
    "TrueWirelessHeadphonesDeviceType.FastPairNotificationType";
constexpr char kFastPairEngagementFlowMetricSubsequentWithFakeMetadata[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps."
    "SubsequentPairingProtocol.TrueWirelessHeadphonesDeviceType."
    "FastPairNotificationType";
constexpr char kInitialSuccessFunnelMetric[] = "FastPair.InitialPairing";
constexpr char kSubsequentSuccessFunnelMetric[] = "FastPair.SubsequentPairing";
const char kFastPairRetroactiveEngagementFlowMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactiveEngagementFunnel.Steps";
const char kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata[] =
    "Bluetooth.ChromeOS.FastPair.RetroactiveEngagementFunnel.Steps."
    "TrueWirelessHeadphonesDeviceType.FastPairNotificationType";
constexpr char kFastPairPairTimeMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.InitialPairingProtocol2";
constexpr char kFastPairPairTimeMetricSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.SubsequentPairingProtocol2";
const char kPairingMethodMetric[] = "Bluetooth.ChromeOS.FastPair.PairingMethod";
const char kRetroactivePairingResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactivePairing.Result";
const char kFastPairPairFailureMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.InitialPairingProtocol";
const char kFastPairPairFailureMetricSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.SubsequentPairingProtocol";
const char kFastPairPairFailureMetricRetroactive[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.RetroactivePairingProtocol";
const char kFastPairPairResultMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.InitialPairingProtocol";
const char kFastPairPairResultMetricSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.SubsequentPairingProtocol";
const char kFastPairPairResultMetricRetroactive[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteResultMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "InitialPairingProtocol";
const char kFastPairAccountKeyWriteResultMetricRetroactive[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteFailureMetricInitial[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Failure.InitialPairingProtocol";
const char kFastPairAccountKeyWriteFailureMetricRetroactive[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Failure.RetroactivePairingProtocol";
constexpr char kRetroactiveSuccessFunnelMetric[] =
    "FastPair.RetroactivePairing";
constexpr char kInitializePairingProcessInitial[] =
    "FastPair.InitialPairing.Initialization";
constexpr char kInitializePairingProcessSubsequent[] =
    "FastPair.SubsequentPairing.Initialization";
constexpr char kInitializePairingProcessRetroactive[] =
    "FastPair.RetroactivePairing.Initialization";

constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kTestBleDeviceName[] = "Test Device Name";
constexpr char kValidModelId[] = "718c17";

constexpr char kUserEmail[] = "user@email";

std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
CreateTestBluetoothDevice(std::string address) {
  return std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
      /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName, address,
      /*paired=*/true, /*connected=*/false);
}

class FakeMetricBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address) {
        return it.get();
      }
    }

    return nullptr;
  }

  void NotifyDevicePairedChanged(device::BluetoothDevice* device,
                                 bool new_paired_status) {
    device::BluetoothAdapter::NotifyDevicePairedChanged(device,
                                                        new_paired_status);
  }

  bool IsPresent() const override { return true; }

  device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
  GetLowEnergyScanSessionHardwareOffloadingStatus() override {
    return device::BluetoothAdapter::
        LowEnergyScanSessionHardwareOffloadingStatus::kSupported;
  }

 private:
  ~FakeMetricBluetoothAdapter() = default;
};

}  // namespace

namespace ash {
namespace quick_pair {

class QuickPairMetricsLoggerTest : public NoSessionAshTestBase {
 public:
  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    // Inject our own PrefServices for each user which enables us to setup the
    // desks restore data before the user signs in.
    auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
    user_prefs_ = user_prefs.get();
    RegisterUserProfilePrefs(user_prefs_->registry(), /*country=*/"",
                             /*for_test=*/true);

    auto accountId = AccountId::FromUserEmail(kUserEmail);
    session_controller->AddUserSession(kUserEmail,
                                       user_manager::UserType::kRegular,
                                       /*provide_pref_service=*/false);
    session_controller->SetUserPrefService(accountId, std::move(user_prefs));

    user_prefs_->registry()->RegisterBooleanPref(
        ash::prefs::kUserPairedWithFastPair,
        /*default_value=*/false);

    adapter_ = base::MakeRefCounted<FakeMetricBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    fake_fast_pair_repository_ = std::make_unique<FakeFastPairRepository>();
    nearby::fastpair::Device metadata;
    metadata.set_notification_type(
        nearby::fastpair::NotificationType::FAST_PAIR);
    metadata.set_device_type(
        nearby::fastpair::DeviceType::TRUE_WIRELESS_HEADPHONES);
    fake_fast_pair_repository_->SetFakeMetadata(kValidModelId, metadata);

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

    initial_device_ = base::MakeRefCounted<Device>(kValidModelId, kTestAddress,
                                                   Protocol::kFastPairInitial);
    subsequent_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestAddress, Protocol::kFastPairSubsequent);
    retroactive_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestAddress, Protocol::kFastPairRetroactive);

    browser_delegate_ = std::make_unique<MockQuickPairBrowserDelegate>();
    ON_CALL(*browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(&pref_service_));
    pref_service_.registry()->RegisterBooleanPref(ash::prefs::kFastPairEnabled,
                                                  /*default_value=*/true);

    metrics_logger_ = std::make_unique<QuickPairMetricsLogger>(
        scanner_broker_.get(), pairer_broker_.get(), ui_broker_.get(),
        retroactive_pairing_detector_.get());
  }

  void TearDown() override { NoSessionAshTestBase::TearDown(); }

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
        mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                               DiscoveryAction::kDismissedByOs);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                               DiscoveryAction::kDismissedByOs);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateDiscoveryUiDismissedByUser(Protocol protocol) {
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

  void SimulateDiscoveryUiDismissedByTimeout(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyDiscoveryAction(
            initial_device_, DiscoveryAction::kDismissedByTimeout);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyDiscoveryAction(
            subsequent_device_, DiscoveryAction::kDismissedByTimeout);
        break;
      case Protocol::kFastPairRetroactive:
        break;
    }
  }

  void SimulateDiscoveryUiLearnMorePressed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                               DiscoveryAction::kLearnMore);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                               DiscoveryAction::kLearnMore);
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
        mock_pairer_broker_->NotifyPairFailure(
            retroactive_device_,
            PairFailure::kKeyBasedPairingCharacteristicDiscovery);
        break;
    }
  }

  void SimulatePairingSucceeded(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        initial_device_->set_classic_address(kTestAddress);
        mock_pairer_broker_->NotifyDevicePaired(initial_device_);
        break;
      case Protocol::kFastPairSubsequent:
        subsequent_device_->set_classic_address(kTestAddress);
        mock_pairer_broker_->NotifyDevicePaired(subsequent_device_);
        break;
      case Protocol::kFastPairRetroactive:
        retroactive_device_->set_classic_address(kTestAddress);
        mock_pairer_broker_->NotifyDevicePaired(retroactive_device_);
        break;
    }
  }

  void SimulatePairingFlow(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyDiscoveryAction(subsequent_device_,
                                               DiscoveryAction::kPairToDevice);
        mock_pairer_broker_->NotifyPairingStart(subsequent_device_);
        mock_pairer_broker_->NotifyHandshakeComplete(subsequent_device_);
        mock_pairer_broker_->NotifyDevicePaired(initial_device_);
        mock_pairer_broker_->NotifyPairComplete(subsequent_device_);
        break;
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyDiscoveryAction(initial_device_,
                                               DiscoveryAction::kPairToDevice);
        mock_pairer_broker_->NotifyPairingStart(initial_device_);
        mock_pairer_broker_->NotifyHandshakeComplete(initial_device_);
        mock_pairer_broker_->NotifyDevicePaired(initial_device_);
        mock_pairer_broker_->NotifyAccountKeyWrite(initial_device_,
                                                   /*error=*/std::nullopt);
        mock_pairer_broker_->NotifyPairComplete(initial_device_);
        break;
      case Protocol::kFastPairRetroactive:
        fake_retroactive_pairing_detector_->NotifyRetroactivePairFound(
            retroactive_device_);
        mock_ui_broker_->NotifyAssociateAccountAction(
            retroactive_device_, AssociateAccountAction::kAssociateAccount);
        mock_pairer_broker_->NotifyPairingStart(retroactive_device_);
        mock_pairer_broker_->NotifyHandshakeComplete(retroactive_device_);
        mock_pairer_broker_->NotifyAccountKeyWrite(retroactive_device_,
                                                   /*error=*/std::nullopt);
        break;
    }
  }

  void SimulateErrorUiDismissedByUser(Protocol protocol) {
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

  void SimulateErrorUiDismissed(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_ui_broker_->NotifyPairingFailedAction(
            initial_device_, PairingFailedAction::kDismissed);
        break;
      case Protocol::kFastPairSubsequent:
        mock_ui_broker_->NotifyPairingFailedAction(
            subsequent_device_, PairingFailedAction::kDismissed);
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
        retroactive_device_, AssociateAccountAction::kDismissedByOs);
  }

  void SimulateAssociateAccountUiDismissedByUser() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kDismissedByUser);
  }

  void SimulateAssociateAccountUiDismissedByTimeout() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kDismissedByTimeout);
  }

  void SimulateAssociateAccountUiSavePressed() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kAssociateAccount);
  }

  void SimulateAssociateAccountUiLearnMorePressed() {
    mock_ui_broker_->NotifyAssociateAccountAction(
        retroactive_device_, AssociateAccountAction::kLearnMore);
  }

  void SimulateAccountKeyWritten(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_pairer_broker_->NotifyAccountKeyWrite(initial_device_,
                                                   std::nullopt);
        break;
      case Protocol::kFastPairSubsequent:
        break;
      case Protocol::kFastPairRetroactive:
        mock_pairer_broker_->NotifyAccountKeyWrite(retroactive_device_,
                                                   std::nullopt);
        break;
    }
  }

  void SimulateAccountKeyFailure(Protocol protocol) {
    switch (protocol) {
      case Protocol::kFastPairInitial:
        mock_pairer_broker_->NotifyAccountKeyWrite(
            initial_device_, AccountKeyFailure::kGattErrorFailed);
        break;
      case Protocol::kFastPairSubsequent:
        break;
      case Protocol::kFastPairRetroactive:
        mock_pairer_broker_->NotifyAccountKeyWrite(
            retroactive_device_, AccountKeyFailure::kGattErrorFailed);
        break;
    }
  }

  void PairFastPairDeviceWithFastPair(std::string address) {
    auto fp_device = base::MakeRefCounted<Device>(kValidModelId, address,
                                                  Protocol::kFastPairInitial);
    fp_device->set_classic_address(address);
    mock_pairer_broker_->NotifyDevicePaired(fp_device);
  }

  void PairFastPairDeviceWithClassicBluetooth(bool new_paired_status,
                                              std::string classic_address) {
    std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
        bluetooth_device = CreateTestBluetoothDevice(classic_address);
    bluetooth_device->AddUUID(ash::quick_pair::kFastPairBluetoothUuid);
    auto* bt_device_ptr = bluetooth_device.get();
    adapter_->AddMockDevice(std::move(bluetooth_device));
    adapter_->NotifyDevicePairedChanged(bt_device_ptr, new_paired_status);
  }

  void AssertUserPairedWithFastPairPref(bool pref_value) {
    EXPECT_EQ(Shell::Get()
                  ->session_controller()
                  ->GetLastActiveUserPrefService()
                  ->GetBoolean(ash::prefs::kUserPairedWithFastPair),
              pref_value);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  base::HistogramTester histogram_tester_;
  scoped_refptr<FakeMetricBluetoothAdapter> adapter_;
  scoped_refptr<Device> initial_device_;
  scoped_refptr<Device> subsequent_device_;
  scoped_refptr<Device> retroactive_device_;

  std::unique_ptr<MockQuickPairBrowserDelegate> browser_delegate_;
  TestingPrefServiceSimple pref_service_;
  raw_ptr<TestingPrefServiceSimple, DanglingUntriaged> user_prefs_;

  raw_ptr<MockScannerBroker, DanglingUntriaged> mock_scanner_broker_ = nullptr;
  raw_ptr<MockPairerBroker, DanglingUntriaged> mock_pairer_broker_ = nullptr;
  raw_ptr<MockUIBroker, DanglingUntriaged> mock_ui_broker_ = nullptr;
  raw_ptr<FakeRetroactivePairingDetector, DanglingUntriaged>
      fake_retroactive_pairing_detector_ = nullptr;

  std::unique_ptr<FakeFastPairRepository> fake_fast_pair_repository_;
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
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiDismissedByUser_Initial) {
  SimulateDiscoveryUiDismissedByUser(Protocol::kFastPairInitial);
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
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiDismissedByTimeout_Initial) {
  SimulateDiscoveryUiDismissedByTimeout(Protocol::kFastPairInitial);
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
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            1);
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
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiDismissedByUser_Subsequent) {
  SimulateDiscoveryUiDismissedByUser(Protocol::kFastPairSubsequent);
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
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiDismissedByTimeout_Subsequent) {
  SimulateDiscoveryUiDismissedByTimeout(Protocol::kFastPairSubsequent);
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
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiConnectPressed_Initial) {
  AssertUserPairedWithFastPairPref(false);
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
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
  AssertUserPairedWithFastPairPref(true);
}

TEST_F(QuickPairMetricsLoggerTest, LogDiscoveryUiConnectPressed_Subsequent) {
  AssertUserPairedWithFastPairPref(false);
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
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
  AssertUserPairedWithFastPairPref(true);
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
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogSuccessFunnel_Initial) {
  SimulatePairingFlow(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kNotificationsClicked),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kInitializationStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPairingStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kPairingComplete),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitialSuccessFunnelMetric,
                FastPairInitialSuccessFunnelEvent::kProcessComplete),
            1);

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessInitial,
                FastPairInitializePairingProcessEvent::kInitializationStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessInitial,
                FastPairInitializePairingProcessEvent::kInitializationComplete),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, LogSuccessFunnel_Retroactive) {
  SimulatePairingFlow(Protocol::kFastPairRetroactive);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kDeviceDetected),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kInitializationStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kWritingAccountKey),
            1);
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kRetroactiveSuccessFunnelMetric,
          FastPairRetroactiveSuccessFunnelEvent::kAccountKeyWrittenToDevice),
      1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kSaveRequested),
            1);

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessRetroactive,
                FastPairInitializePairingProcessEvent::kInitializationStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessRetroactive,
                FastPairInitializePairingProcessEvent::kInitializationComplete),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, LogSuccessFunnel_Subseqent) {
  SimulatePairingFlow(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kSubsequentSuccessFunnelMetric,
                FastPairSubsequentSuccessFunnelEvent::kNotificationsClicked),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kSubsequentSuccessFunnelMetric,
                FastPairSubsequentSuccessFunnelEvent::kInitializationStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kSubsequentSuccessFunnelMetric,
                FastPairSubsequentSuccessFunnelEvent::kPairingStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kSubsequentSuccessFunnelMetric,
                FastPairSubsequentSuccessFunnelEvent::kProcessComplete),
            1);

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessSubsequent,
                FastPairInitializePairingProcessEvent::kInitializationStarted),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kInitializePairingProcessSubsequent,
                FastPairInitializePairingProcessEvent::kInitializationComplete),
            1);
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
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiDismissedByUser_Initial) {
  SimulateErrorUiDismissedByUser(Protocol::kFastPairInitial);
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
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
                FastPairEngagementFlowEvent::kErrorUiDismissed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogErrorUiDismissedByUser_Subsequent) {
  SimulateErrorUiDismissedByUser(Protocol::kFastPairSubsequent);
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
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitialWithFakeMetadata,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequentWithFakeMetadata,
                FastPairEngagementFlowEvent::kErrorUiSettingsPressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kErrorUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByUser),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiDismissedByTimeout),
            0);
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
          kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
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
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByUser),
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountDismissedByTimeout) {
  SimulateAssociateAccountUiDismissedByTimeout();
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, LogAssociateAccountSavePressed) {
  AssertUserPairedWithFastPairPref(false);
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
  EXPECT_EQ(
      histogram_tester().GetBucketCount(
          kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
            0);
  AssertUserPairedWithFastPairPref(true);
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
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
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
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
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
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
            0);
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
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
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
                kFastPairRetroactiveEngagementFlowMetricWithFakeMetadata,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedByUserAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountDismissedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairRetroactiveEngagementFlowMetric,
                FastPairRetroactiveEngagementFlowEvent::
                    kAssociateAccountUiDismissedByTimeout),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, DevicedPaired_FastPair) {
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kFastPair),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kSystemPairingUi),
            0);
  PairFastPairDeviceWithFastPair(kTestDeviceAddress);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kFastPair),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kSystemPairingUi),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, DeviceUnpaired) {
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kFastPair),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kSystemPairingUi),
            0);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/false, kTestDeviceAddress);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kFastPair),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kSystemPairingUi),
            0);
}

TEST_F(QuickPairMetricsLoggerTest, DevicePaired) {
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kFastPair),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kSystemPairingUi),
            0);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kFastPair),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(kPairingMethodMetric,
                                              PairingMethod::kSystemPairingUi),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, WriteAccountKey_Initial) {
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
  SimulateAccountKeyWritten(Protocol::kFastPairInitial);
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, WriteAccountKey_Retroactive) {
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
  SimulateAccountKeyWritten(Protocol::kFastPairRetroactive);
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, WriteAccountKeyFailure_Retroactive) {
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
  SimulateAccountKeyFailure(Protocol::kFastPairRetroactive);
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 1);
}

TEST_F(QuickPairMetricsLoggerTest, WriteAccountKeyFailure_Initial) {
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
  SimulateAccountKeyFailure(Protocol::kFastPairInitial);
  histogram_tester().ExpectTotalCount(kRetroactivePairingResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricInitial, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricInitial, 1);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteResultMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(
      kFastPairAccountKeyWriteFailureMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairFailure_Initial) {
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);

  SimulatePairingFailed(Protocol::kFastPairInitial);

  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairSuccess_Initial) {
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);

  SimulatePairingSucceeded(Protocol::kFastPairInitial);

  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairFailure_Subsequent) {
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);

  SimulatePairingFailed(Protocol::kFastPairSubsequent);

  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairSuccess_Subsequent) {
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);

  SimulatePairingSucceeded(Protocol::kFastPairSubsequent);

  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairFailure_Retroactive) {
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);

  SimulatePairingFailed(Protocol::kFastPairRetroactive);

  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 1);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 1);
}

TEST_F(QuickPairMetricsLoggerTest, LogPairSuccess_Retroactive) {
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 0);

  SimulatePairingSucceeded(Protocol::kFastPairRetroactive);

  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairFailureMetricRetroactive, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricInitial, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricSubsequent, 0);
  histogram_tester().ExpectTotalCount(kFastPairPairResultMetricRetroactive, 1);
}

TEST_F(QuickPairMetricsLoggerTest, DiscoveryLearnMorePressed_Initial) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiLearnMorePressed_ConnectPressed_Initial) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  SimulateDiscoveryUiConnectPressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiLearnMorePressed_DismissedByUser_Initial) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  SimulateDiscoveryUiDismissedByUser(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiLearnMorePressed_Dismissed_Initial) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  SimulateDiscoveryUiDismissed(Protocol::kFastPairInitial);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricInitial,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            1);
}

TEST_F(QuickPairMetricsLoggerTest, DiscoveryLearnMorePressed_Subsequent) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiLearnMorePressed_ConnectPressed_Subsequent) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  SimulateDiscoveryUiConnectPressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiLearnMorePressed_DismissedByUser_Subsequent) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  SimulateDiscoveryUiDismissedByUser(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            0);
}

TEST_F(QuickPairMetricsLoggerTest,
       LogDiscoveryUiLearnMorePressed_Dismissed_Subsequent) {
  SimulateDiscoveryUiLearnMorePressed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  SimulateDiscoveryUiDismissed(Protocol::kFastPairSubsequent);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::kDiscoveryUiLearnMorePressed),
            1);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiConnectPressedAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedByUserAfterLearnMorePressed),
            0);
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kFastPairEngagementFlowMetricSubsequent,
                FastPairEngagementFlowEvent::
                    kDiscoveryUiDismissedAfterLearnMorePressed),
            1);
}

}  // namespace quick_pair
}  // namespace ash
