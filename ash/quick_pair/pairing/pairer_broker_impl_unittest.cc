// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/pairer_broker_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_bluetooth_adapter.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake_lookup.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kValidModelId[] = "718c17";
constexpr char kTestDeviceAddress[] = "test_address";
constexpr char kTestDeviceAddress2[] = "test_address_2";
constexpr char kDeviceName[] = "test_device_name";
constexpr char kBluetoothCanonicalizedAddress[] = "0C:0E:4C:C8:05:08";
const uint8_t kValidPasskey = 13;
constexpr base::TimeDelta kCancelPairingRetryDelay = base::Seconds(1);

const char kFastPairRetryCountMetricName[] =
    "Bluetooth.ChromeOS.FastPair.PairRetry.Count";
constexpr char kInitializePairingProcessInitial[] =
    "FastPair.InitialPairing.Initialization";
constexpr char kInitializePairingProcessSubsequent[] =
    "FastPair.SubsequentPairing.Initialization";
constexpr char kInitializePairingProcessRetroactive[] =
    "FastPair.RetroactivePairing.Initialization";
constexpr char kInitializePairingProcessFailureReasonInitial[] =
    "FastPair.InitialPairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessFailureReasonSubsequent[] =
    "FastPair.SubsequentPairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessFailureReasonRetroactive[] =
    "FastPair.RetroactivePairing.Initialization.FailureReason";

constexpr char kProtocolPairingStepInitial[] =
    "FastPair.InitialPairing.Pairing";
constexpr char kProtocolPairingStepSubsequent[] =
    "FastPair.SubsequentPairing.Pairing";
const char kHandshakeEffectiveSuccessRate[] =
    "FastPair.Handshake.EffectiveSuccessRate";
const char kHandshakeAttemptCount[] = "FastPair.Handshake.AttemptCount";

class FakeFastPairPairer : public ash::quick_pair::FastPairPairer {
 public:
  FakeFastPairPairer(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<ash::quick_pair::Device> device,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          paired_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          pairing_procedure_complete)
      : adapter_(adapter),
        device_(device),
        paired_callback_(std::move(paired_callback)),
        pair_failed_callback_(std::move(pair_failed_callback)),
        account_key_failure_callback_(std::move(account_key_failure_callback)),
        display_passkey_(std::move(display_passkey)),
        pairing_procedure_complete_(std::move(pairing_procedure_complete)) {}

  ~FakeFastPairPairer() override = default;

  void TriggerPairedCallback() {
    EXPECT_TRUE(paired_callback_);
    std::move(paired_callback_).Run(device_);
  }

  void TriggerPairingProcedureCompleteCallback() {
    EXPECT_TRUE(pairing_procedure_complete_);
    std::move(pairing_procedure_complete_).Run(device_);
  }

  void TriggerAccountKeyFailureCallback(
      ash::quick_pair::AccountKeyFailure failure) {
    EXPECT_TRUE(account_key_failure_callback_);
    std::move(account_key_failure_callback_).Run(device_, failure);
  }

  void TriggerPairFailureCallback(ash::quick_pair::PairFailure failure) {
    EXPECT_TRUE(pair_failed_callback_);
    std::move(pair_failed_callback_).Run(device_, failure);
  }

  void TriggerDisplayPasskeyCallback() {
    EXPECT_TRUE(display_passkey_);
    std::move(display_passkey_).Run(std::u16string(), kValidPasskey);
  }

 private:
  scoped_refptr<device::BluetoothAdapter> adapter_;
  scoped_refptr<ash::quick_pair::Device> device_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
      paired_callback_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                          ash::quick_pair::PairFailure)>
      pair_failed_callback_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                          ash::quick_pair::AccountKeyFailure)>
      account_key_failure_callback_;
  base::OnceCallback<void(std::u16string, uint32_t)> display_passkey_;
  base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
      pairing_procedure_complete_;
};

class FakeFastPairPairerFactory
    : public ash::quick_pair::FastPairPairerImpl::Factory {
 public:
  std::unique_ptr<ash::quick_pair::FastPairPairer> CreateInstance(
      scoped_refptr<device::BluetoothAdapter> adapter,
      scoped_refptr<ash::quick_pair::Device> device,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          paired_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::PairFailure)>
          pair_failed_callback,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>,
                              ash::quick_pair::AccountKeyFailure)>
          account_key_failure_callback,
      base::OnceCallback<void(std::u16string, uint32_t)> display_passkey,
      base::OnceCallback<void(scoped_refptr<ash::quick_pair::Device>)>
          pairing_procedure_complete) override {
    auto fake_fast_pair_pairer = std::make_unique<FakeFastPairPairer>(
        std::move(adapter), std::move(device), std::move(paired_callback),
        std::move(pair_failed_callback),
        std::move(account_key_failure_callback), std::move(display_passkey),
        std::move(pairing_procedure_complete));
    fake_fast_pair_pairer_ = fake_fast_pair_pairer.get();
    return fake_fast_pair_pairer;
  }

  ~FakeFastPairPairerFactory() override = default;

  FakeFastPairPairer* fake_fast_pair_pairer() { return fake_fast_pair_pairer_; }

 protected:
  raw_ptr<FakeFastPairPairer, DanglingUntriaged> fake_fast_pair_pairer_ =
      nullptr;
};

class FakeFastPairGattServiceClientImplFactory
    : public ash::quick_pair::FastPairGattServiceClientImpl::Factory {
 public:
  ~FakeFastPairGattServiceClientImplFactory() override = default;

  ash::quick_pair::FakeFastPairGattServiceClient*
  fake_fast_pair_gatt_service_client() {
    return fake_fast_pair_gatt_service_client_;
  }

 private:
  // FastPairGattServiceClientImpl::Factory:
  std::unique_ptr<ash::quick_pair::FastPairGattServiceClient> CreateInstance(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(std::optional<ash::quick_pair::PairFailure>)>
          on_initialized_callback) override {
    auto fake_fast_pair_gatt_service_client =
        std::make_unique<ash::quick_pair::FakeFastPairGattServiceClient>(
            device, adapter, std::move(on_initialized_callback));
    fake_fast_pair_gatt_service_client_ =
        fake_fast_pair_gatt_service_client.get();
    return fake_fast_pair_gatt_service_client;
  }

  raw_ptr<ash::quick_pair::FakeFastPairGattServiceClient, DanglingUntriaged>
      fake_fast_pair_gatt_service_client_ = nullptr;
};

}  // namespace

namespace ash {
namespace quick_pair {

class PairerBrokerImplTest : public AshTestBase, public PairerBroker::Observer {
 public:
  PairerBrokerImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    AshTestBase::SetUp();

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    fast_pair_pairer_factory_ = std::make_unique<FakeFastPairPairerFactory>();
    FastPairPairerImpl::Factory::SetFactoryForTesting(
        fast_pair_pairer_factory_.get());

    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);
    FastPairHandshakeLookup::UseFakeInstance();
    pairer_broker_ = std::make_unique<PairerBrokerImpl>();
    pairer_broker_->AddObserver(this);

    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        mock_bluetooth_device_ptr_, adapter_.get(), base::DoNothing());

    // We have to pass in a unique_ptr when we create a Handshake, however
    // we also want to be able to set fake responses on the encryptor. Thus
    // we maintain 2 pointers. We won't touch fake_fast_pair_data_encryptor_
    // aside from CreateHandshake.
    fake_fast_pair_data_encryptor_ =
        std::make_unique<FakeFastPairDataEncryptor>();
  }

  void CreateMockDevice(DeviceFastPairVersion version, Protocol protocol) {
    device_ = base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress,
                                           protocol);
    device_->set_classic_address(kBluetoothCanonicalizedAddress);

    device_->set_version(version);

    // Add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    mock_bluetooth_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), /*bluetooth_class=*/0, kDeviceName,
            kBluetoothCanonicalizedAddress,
            /*initially_paired=*/true, /*connected=*/false);
    mock_bluetooth_device_ptr_ = mock_bluetooth_device_.get();
    adapter_->AddMockDevice(std::move(mock_bluetooth_device_));
  }

  void EraseHandshake() {
    FastPairHandshakeLookup::GetInstance()->Erase(device_);
  }

  void InvokeHandshakeLookupCallbackSuccess() {
    FakeFastPairHandshakeLookup::GetFakeInstance()->InvokeCallbackForTesting(
        device_, std::nullopt);
  }

  void InvokeHandshakeLookupCallbackFailure(PairFailure failure) {
    FakeFastPairHandshakeLookup::GetFakeInstance()->InvokeCallbackForTesting(
        device_, failure);
  }

  void ExpectHandshakeExistsForDevice(scoped_refptr<Device> device) {
    auto* handshake = FastPairHandshakeLookup::GetInstance()->Get(device);
    EXPECT_TRUE(handshake);
  }

  void ExpectBleRotatedForDevice(scoped_refptr<Device> device) {
    auto* handshake = FastPairHandshakeLookup::GetInstance()->Get(device);
    EXPECT_TRUE(handshake->DidBleAddressRotate());
  }

  void TearDown() override {
    pairer_broker_->RemoveObserver(this);
    pairer_broker_.reset();
    AshTestBase::TearDown();
  }

  void OnDevicePaired(scoped_refptr<Device> device) override {
    ++device_paired_count_;
  }

  void OnPairFailure(scoped_refptr<Device> device,
                     PairFailure failure) override {
    ++pair_failure_count_;
  }

  void OnAccountKeyWrite(scoped_refptr<Device> device,
                         std::optional<AccountKeyFailure> error) override {
    ++account_key_write_count_;
  }

  void OnPairingStart(scoped_refptr<Device> device) override {
    pairing_started_ = true;
  }

  void OnHandshakeComplete(scoped_refptr<Device> device) override {
    handshake_complete_ = true;
  }

  void OnDisplayPasskey(std::u16string device_name, uint32_t passkey) override {
    display_passkey_ = passkey;
  }

  void OnPairingComplete(scoped_refptr<Device> device) override {
    device_pair_complete_ = true;
  }

  void PairerSetCurrentBleAddress(std::string ble_address) {
    pairer_broker_
        ->model_id_to_current_ble_address_map_[device_->metadata_id()] =
        ble_address;
  }

  void PairerCreateHandshake(scoped_refptr<Device> device) {
    pairer_broker_->CreateHandshake(device);
  }

  void PairerOnBleAddressRotation(scoped_refptr<Device> device) {
    pairer_broker_->OnBleAddressRotation(device);
  }

 protected:
  int device_paired_count_ = 0;
  int pair_failure_count_ = 0;
  int account_key_write_count_ = 0;
  uint32_t display_passkey_ = 0;
  bool pairing_started_ = false;
  bool handshake_complete_ = false;
  bool device_pair_complete_ = false;

  base::HistogramTester histogram_tester_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  raw_ptr<device::MockBluetoothDevice> mock_bluetooth_device_ptr_ = nullptr;
  std::unique_ptr<FakeFastPairPairerFactory> fast_pair_pairer_factory_;

  std::unique_ptr<PairerBrokerImpl> pairer_broker_;
  raw_ptr<FakeFastPairHandshake, DanglingUntriaged> fake_fast_pair_handshake_ =
      nullptr;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;
  FakeFastPairGattServiceClientImplFactory fast_pair_gatt_service_factory_;
  std::unique_ptr<FakeFastPairDataEncryptor> fake_fast_pair_data_encryptor_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
      mock_bluetooth_device_ = nullptr;
  scoped_refptr<Device> device_;
};

TEST_F(PairerBrokerImplTest, PairV1Device_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);

  CreateMockDevice(DeviceFastPairVersion::kV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();

  EXPECT_EQ(account_key_write_count_, 0);
}

TEST_F(PairerBrokerImplTest, PairV2Device_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

TEST_F(PairerBrokerImplTest, PairDevice_Subsequent) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairing_started_);
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_TRUE(device_pair_complete_);
}

TEST_F(PairerBrokerImplTest, Ble_Address_Matches_Create_Handshake) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({ash::features::kFastPairBleRotation}, {});

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // Populate the ble_address map with the correct address.
  PairerSetCurrentBleAddress(device_->ble_address());
  PairerCreateHandshake(device_);
  ExpectHandshakeExistsForDevice(device_);
}

TEST_F(PairerBrokerImplTest, Ble_Address_Mismatch_No_Handshake) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairBleRotation};
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // Populate the ble_address map with a different address than the current BLE
  // address.
  PairerSetCurrentBleAddress(kTestDeviceAddress2);
  PairerCreateHandshake(device_);
  EXPECT_EQ(fake_fast_pair_handshake_, nullptr);
}

TEST_F(PairerBrokerImplTest, Ble_Address_Mismatch_Set_Callback) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairBleRotation};
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  // Populate the ble_address map with a different address than the current BLE
  // address.
  PairerSetCurrentBleAddress(device_->ble_address());
  PairerCreateHandshake(device_);

  scoped_refptr<Device> device_after_ble_rotation_ =
      base::MakeRefCounted<Device>(kValidModelId, kTestDeviceAddress2,
                                   Protocol::kFastPairRetroactive);
  device_after_ble_rotation_->set_classic_address(
      kBluetoothCanonicalizedAddress);
  device_after_ble_rotation_->set_version(DeviceFastPairVersion::kHigherThanV1);

  pairer_broker_->PairDevice(device_after_ble_rotation_);

  ExpectBleRotatedForDevice(device_);
}

TEST_F(PairerBrokerImplTest, OnBleAddressRotation_Pairs_Successfully) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({ash::features::kFastPairBleRotation}, {});

  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  // Populate the ble_address map with the correct address.
  PairerSetCurrentBleAddress(device_->ble_address());

  // Call PairerOnBleAddressRotation, this is analogous to the function being
  // called as a callback from the pairer.
  PairerOnBleAddressRotation(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, PairDevice_Retroactive) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, AlreadyPairingDevice_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();
  pairer_broker_->PairDevice(device_);
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessInitial,
                FastPairInitializePairingProcessEvent::kAlreadyPairingFailure),
            1);
}

TEST_F(PairerBrokerImplTest, AlreadyPairingDevice_Subsequent) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  pairer_broker_->PairDevice(device_);

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessSubsequent,
                FastPairInitializePairingProcessEvent::kAlreadyPairingFailure),
            1);
}

TEST_F(PairerBrokerImplTest, AlreadyPairingDevice_Retroactive) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  pairer_broker_->PairDevice(device_);

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessRetroactive,
                FastPairInitializePairingProcessEvent::kAlreadyPairingFailure),
            1);
}

TEST_F(PairerBrokerImplTest, PairAfterCancelPairing) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();
  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_CALL(*mock_bluetooth_device_ptr_, IsPaired())
      .WillOnce(testing::Return(false));

  // Attempt to pair with a failure.
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);

  // Fast forward |kCancelPairingDelay| seconds to allow the retry callback to
  // be called.
  task_environment()->FastForwardBy(kCancelPairingRetryDelay);

  // Now allow the pairing to succeed.
  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);
}

TEST_F(PairerBrokerImplTest, PairDeviceFailureMax_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  histogram_tester_.ExpectTotalCount(kProtocolPairingStepInitial, 1);
}

TEST_F(PairerBrokerImplTest, PairDeviceFailureMax_Subsequent) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  histogram_tester_.ExpectTotalCount(kProtocolPairingStepSubsequent, 1);
}

TEST_F(PairerBrokerImplTest, PairDeviceFailureMax_Retroactive) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);
  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairFailureCallback(
          PairFailure::kPasskeyCharacteristicNotifySession);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
}

TEST_F(PairerBrokerImplTest, AccountKeyFailure_Initial) {
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerAccountKeyFailureCallback(
          AccountKeyFailure::kAccountKeyCharacteristicDiscovery);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

TEST_F(PairerBrokerImplTest, AccountKeyFailure_Subsequent) {
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerAccountKeyFailureCallback(
          AccountKeyFailure::kAccountKeyCharacteristicDiscovery);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

TEST_F(PairerBrokerImplTest, AccountKeyFailure_Retroactive) {
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerAccountKeyFailureCallback(
          AccountKeyFailure::kAccountKeyCharacteristicDiscovery);

  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(account_key_write_count_, 1);
}

TEST_F(PairerBrokerImplTest, StopPairing) {
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  // Stop Pairing mid pair.
  pairer_broker_->StopPairing();
  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(pair_failure_count_, 0);

  // Stop Pairing when we are not pairing should cause no issues.
  pairer_broker_->StopPairing();
  EXPECT_FALSE(pairer_broker_->IsPairing());
}

TEST_F(PairerBrokerImplTest, ReuseHandshake_Initial) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);

  FakeFastPairHandshakeLookup::GetFakeInstance()->CreateForTesting(
      adapter_, device_, base::DoNothing(), nullptr, nullptr);

  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessInitial,
                FastPairInitializePairingProcessEvent::kHandshakeReused),
            1);
}

TEST_F(PairerBrokerImplTest, ReuseHandshake_Subsequent) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);

  FakeFastPairHandshakeLookup::GetFakeInstance()->CreateForTesting(
      adapter_, device_, base::DoNothing(), nullptr, nullptr);

  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairing_started_);
  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  EXPECT_FALSE(pairer_broker_->IsPairing());
  EXPECT_TRUE(device_pair_complete_);

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessSubsequent,
                FastPairInitializePairingProcessEvent::kHandshakeReused),
            1);
}

TEST_F(PairerBrokerImplTest, ReuseHandshake_Retroactive) {
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 0);
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);

  FakeFastPairHandshakeLookup::GetFakeInstance()->CreateForTesting(
      adapter_, device_, base::DoNothing(), nullptr, nullptr);

  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()->TriggerPairedCallback();

  EXPECT_TRUE(pairer_broker_->IsPairing());
  EXPECT_EQ(device_paired_count_, 1);
  histogram_tester_.ExpectTotalCount(kFastPairRetryCountMetricName, 1);

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerPairingProcedureCompleteCallback();
  EXPECT_FALSE(pairer_broker_->IsPairing());

  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessRetroactive,
                FastPairInitializePairingProcessEvent::kHandshakeReused),
            1);
}

TEST_F(PairerBrokerImplTest, NoPairingIfHandshakeFailed_Initial) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairHandshakeLongTermRefactor};
  histogram_tester_.ExpectTotalCount(kHandshakeEffectiveSuccessRate, 0);
  histogram_tester_.ExpectTotalCount(kHandshakeAttemptCount, 0);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackFailure(PairFailure::kCreateGattConnection);

  EXPECT_EQ(device_paired_count_, 0);
  EXPECT_EQ(pair_failure_count_, 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessFailureReasonInitial,
                PairFailure::kCreateGattConnection),
            1);
}

TEST_F(PairerBrokerImplTest, NoPairingIfHandshakeFailed_Subsequent) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairHandshakeLongTermRefactor};
  histogram_tester_.ExpectTotalCount(kHandshakeEffectiveSuccessRate, 0);
  histogram_tester_.ExpectTotalCount(kHandshakeAttemptCount, 0);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairSubsequent);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackFailure(PairFailure::kCreateGattConnection);

  EXPECT_EQ(device_paired_count_, 0);
  EXPECT_EQ(pair_failure_count_, 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessFailureReasonSubsequent,
                PairFailure::kCreateGattConnection),
            1);
}

TEST_F(PairerBrokerImplTest, NoPairingIfHandshakeFailed_Retroactive) {
  base::test::ScopedFeatureList feature_list{
      ash::features::kFastPairHandshakeLongTermRefactor};
  histogram_tester_.ExpectTotalCount(kHandshakeEffectiveSuccessRate, 0);
  histogram_tester_.ExpectTotalCount(kHandshakeAttemptCount, 0);

  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairRetroactive);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackFailure(PairFailure::kCreateGattConnection);

  EXPECT_EQ(device_paired_count_, 0);
  EXPECT_EQ(pair_failure_count_, 1);
  EXPECT_EQ(histogram_tester_.GetBucketCount(
                kInitializePairingProcessFailureReasonRetroactive,
                PairFailure::kCreateGattConnection),
            1);
}

TEST_F(PairerBrokerImplTest, DisplayPasskeySuccess) {
  CreateMockDevice(DeviceFastPairVersion::kHigherThanV1,
                   /*protocol=*/Protocol::kFastPairInitial);
  pairer_broker_->PairDevice(device_);
  InvokeHandshakeLookupCallbackSuccess();

  EXPECT_TRUE(pairer_broker_->IsPairing());

  fast_pair_pairer_factory_->fake_fast_pair_pairer()
      ->TriggerDisplayPasskeyCallback();

  EXPECT_EQ(display_passkey_, kValidPasskey);
}

}  // namespace quick_pair
}  // namespace ash
