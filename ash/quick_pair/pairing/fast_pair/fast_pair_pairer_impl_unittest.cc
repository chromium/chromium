// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer_impl.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const std::vector<uint8_t> kResponseBytes = {0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
                                             0x32, 0x1D, 0xA0, 0xBA, 0xF0, 0xBB,
                                             0x95, 0x1F, 0xF7, 0xB6};
std::array<uint8_t, 12> kPasskeySaltBytes = {
    0xF0, 0xBB, 0x95, 0x1F, 0xF7, 0xB6, 0xBA, 0xF0, 0xBB, 0xB6, 0xBA, 0xF0};

const std::array<uint8_t, 64> kPublicKey = {
    0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F,
    0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
    0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01,
    0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45,
    0x61, 0xC3, 0x32, 0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32,
    0x1D, 0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3, 0x32, 0x1D};

const uint8_t kValidPasskey = 13;
const uint8_t kInvalidPasskey = 9;

constexpr char kMetadataId[] = "test_metadata_id";
constexpr char kDeviceName[] = "test_device_name";
constexpr char kBluetoothCanonicalizedAddress[] = "0C:0E:4C:C8:05:08";
const std::string kUserEmail = "test@test.test";

const char kWritePasskeyCharacteristicResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.Result";
const char kWritePasskeyCharacteristicPairFailureMetric[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.PairFailure";
const char kPasskeyCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Time";
const char kPasskeyCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Result";
const char kWriteAccountKeyCharacteristicResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result";
const char kConnectDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.ConnectDevice.Result";
const char kPairDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.Result";
const char kPairDeviceErrorReason[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.ErrorReason";
const char kConfirmPasskeyAskTime[] =
    "Bluetooth.ChromeOS.FastPair.RequestPasskey.Latency";
const char kConfirmPasskeyConfirmTime[] =
    "Bluetooth.ChromeOS.FastPair.ConfirmPasskey.Latency";

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    // To mock when we the device is lost before pairing begins.
    if (get_device_initial_failure_) {
      get_device_initial_failure_ = false;
      return nullptr;
    }

    // To mock when we need to pair to address. We need an initial return of a
    // device to get this point, but the following call should return a nullptr
    // to demonstrate that we lost the device.
    if (get_device_connect_failure_) {
      get_device_initial_failure_ = true;
    }

    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }

    return nullptr;
  }

  void AddPairingDelegate(
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      PairingDelegatePriority priority) override {
    pairing_delegate_ = pairing_delegate;
  }

  void NotifyGattDiscoveryCompleteForService(
      device::BluetoothRemoteGattService* service) {
    device::BluetoothAdapter::NotifyGattDiscoveryComplete(service);
  }

  void NotifyConfirmPasskey(uint32_t passkey, device::BluetoothDevice* device) {
    pairing_delegate_->ConfirmPasskey(device, passkey);
  }

  void DevicePairedChanged(device::BluetoothDevice* device,
                           bool new_paired_status) {
    for (auto& observer : GetObservers())
      observer.DevicePairedChanged(this, device, new_paired_status);
  }

  void SetGetDeviceInitialFailure() { get_device_initial_failure_ = true; }

  void SetGetDeviceInitialSuccess() { get_device_initial_failure_ = false; }

  void SetGetDeviceConnectFailure() { get_device_connect_failure_ = true; }

  void ConnectDevice(
      const std::string& address,
      const absl::optional<device::BluetoothDevice::AddressType>& address_type,
      base::OnceCallback<void(device::BluetoothDevice*)> callback,
      base::OnceClosure error_callback) override {
    if (connect_device_failure_) {
      std::move(error_callback).Run();
      return;
    }

    std::move(callback).Run(GetDevice(address));
  }

  void SetConnectFailure() { connect_device_failure_ = true; }

 protected:
  ~FakeBluetoothAdapter() override = default;
  bool get_device_initial_failure_ = false;
  bool connect_device_failure_ = false;
  bool get_device_connect_failure_ = false;
  device::BluetoothDevice::PairingDelegate* pairing_delegate_ = nullptr;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  explicit FakeBluetoothDevice(FakeBluetoothAdapter* adapter)
      : testing::NiceMock<device::MockBluetoothDevice>(
            adapter,
            0,
            kDeviceName,
            kBluetoothCanonicalizedAddress,
            /*paired=*/true,
            /*connected*/ false),
        fake_adapter_(adapter) {}

  // Move-only class
  FakeBluetoothDevice(const FakeBluetoothDevice&) = delete;
  FakeBluetoothDevice& operator=(const FakeBluetoothDevice&) = delete;

  void Pair(
      BluetoothDevice::PairingDelegate* pairing_delegate,
      base::OnceCallback<void(absl::optional<ConnectErrorCode> error_code)>
          callback) override {
    if (pair_failure_) {
      std::move(callback).Run(ConnectErrorCode::ERROR_FAILED);
      return;
    }

    std::move(callback).Run(absl::nullopt);
  }

  void SetPairFailure() { pair_failure_ = true; }

  void ConfirmPairing() override { is_device_paired_ = true; }

  bool IsDevicePaired() { return is_device_paired_; }

 protected:
  FakeBluetoothAdapter* fake_adapter_;
  bool pair_failure_ = false;
  bool is_device_paired_ = false;
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
      base::OnceCallback<void(absl::optional<ash::quick_pair::PairFailure>)>
          on_initialized_callback) override {
    auto fake_fast_pair_gatt_service_client =
        std::make_unique<ash::quick_pair::FakeFastPairGattServiceClient>(
            device, adapter, std::move(on_initialized_callback));
    fake_fast_pair_gatt_service_client_ =
        fake_fast_pair_gatt_service_client.get();
    return fake_fast_pair_gatt_service_client;
  }

  ash::quick_pair::FakeFastPairGattServiceClient*
      fake_fast_pair_gatt_service_client_ = nullptr;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairPairerImplTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);
  }

  void TearDown() override {
    pairer_.reset();
    ClearLogin();
    AshTestBase::TearDown();
  }

  void SuccessfulDataEncryptorSetUp(bool fast_pair_v1, Protocol protocol) {
    device_ = base::MakeRefCounted<Device>(
        kMetadataId, kBluetoothCanonicalizedAddress, protocol);
    device_->set_classic_address(kBluetoothCanonicalizedAddress);

    if (fast_pair_v1) {
      device_->SetAdditionalData(Device::AdditionalDataType::kFastPairVersion,
                                 {1});
    }

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

    // Need to add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    fake_bluetooth_device_ =
        std::make_unique<FakeBluetoothDevice>(adapter_.get());
    fake_bluetooth_device_ptr_ = fake_bluetooth_device_.get();
    adapter_->AddMockDevice(std::move(fake_bluetooth_device_));

    data_encryptor_ = new FakeFastPairDataEncryptor();

    gatt_service_client_ = FastPairGattServiceClientImpl::Factory::Create(
        fake_bluetooth_device_ptr_, adapter_.get(), base::DoNothing());
    FastPairHandshakeLookup::SetCreateFunctionForTesting(base::BindRepeating(
        &FastPairPairerImplTest::CreateHandshake, base::Unretained(this)));
    FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                   base::DoNothing());
  }

  void FailedHandshakeSetUp(Protocol protocol) {
    device_ = base::MakeRefCounted<Device>(
        kMetadataId, kBluetoothCanonicalizedAddress, protocol);
    device_->set_classic_address(kBluetoothCanonicalizedAddress);

    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

    // Need to add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    fake_bluetooth_device_ =
        std::make_unique<FakeBluetoothDevice>(adapter_.get());
    fake_bluetooth_device_ptr_ = fake_bluetooth_device_.get();
    adapter_->AddMockDevice(std::move(fake_bluetooth_device_));
  }

  void EraseHandshake() {
    FastPairHandshakeLookup::GetInstance()->Erase(device_);
  }

  std::unique_ptr<FastPairHandshake> CreateHandshake(
      scoped_refptr<Device> device,
      FastPairHandshake::OnCompleteCallback callback) {
    auto fake = std::make_unique<FakeFastPairHandshake>(
        adapter_, device, std::move(callback),
        base::WrapUnique(data_encryptor_), std::move(gatt_service_client_));
    fake->InvokeCallback();
    return fake;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void SetDecryptPasskeyForIncorrectMessageType(FastPairMessageType type) {
    DecryptedPasskey passkey(type, kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void SetDecryptPasskeyForPasskeyMismatch() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kInvalidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void SetDecryptPasskeyForNoPasskey() {
    data_encryptor_->passkey(absl::nullopt);
  }

  void SetDecryptPasskeyForSuccess() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void RunWritePasskeyCallback(
      std::vector<uint8_t> data,
      absl::optional<PairFailure> failure = absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWritePasskeyCallback(data, failure);
  }

  void RunWriteAccountKeyCallback(
      absl::optional<device::BluetoothGattService::GattErrorCode> error =
          absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWriteAccountKeyCallback(error);
  }

  void PairFailedCallback(scoped_refptr<Device> device, PairFailure failure) {
    failure_ = failure;
  }

  void NotifyConfirmPasskey() {
    adapter_->NotifyConfirmPasskey(kValidPasskey, fake_bluetooth_device_ptr_);
  }

  absl::optional<PairFailure> GetPairFailure() { return failure_; }

  void SetPairFailure() { fake_bluetooth_device_ptr_->SetPairFailure(); }

  void SetGetDeviceInitialFailure() { adapter_->SetGetDeviceInitialFailure(); }

  void SetConnectFailure() { adapter_->SetConnectFailure(); }

  void SetGetDeviceConnectFailure() { adapter_->SetGetDeviceConnectFailure(); }

  void SetGetDeviceInitialSuccess() { adapter_->SetGetDeviceInitialSuccess(); }

  bool IsDevicePaired() { return fake_bluetooth_device_ptr_->IsDevicePaired(); }

  bool IsAccountKeySavedToFootprints() {
    return fast_pair_repository_.HasKeyForDevice(
        fake_bluetooth_device_ptr_->GetAddress());
  }

  void SetPublicKey() { data_encryptor_->public_key(kPublicKey); }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  void DeviceUnpaired() {
    adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, false);
  }

  void DevicePaired() {
    adapter_->DevicePairedChanged(fake_bluetooth_device_ptr_, true);
  }

 protected:
  // This is done on-demand to enable setting up mock expectations first.
  void CreatePairer() {
    pairer_ = std::make_unique<FastPairPairerImpl>(
        adapter_, device_, paired_callback_.Get(),
        base::BindOnce(&FastPairPairerImplTest::PairFailedCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        account_key_failure_callback_.Get(), pairing_procedure_complete_.Get());
  }

  void CreatePairerAsFactory() {
    pairer_ = FastPairPairerImpl::Factory::Create(
        adapter_, device_, paired_callback_.Get(),
        base::BindOnce(&FastPairPairerImplTest::PairFailedCallback,
                       weak_ptr_factory_.GetWeakPtr()),
        account_key_failure_callback_.Get(), pairing_procedure_complete_.Get());
  }

  absl::optional<PairFailure> failure_ = absl::nullopt;
  std::unique_ptr<FakeBluetoothDevice> fake_bluetooth_device_;
  FakeBluetoothDevice* fake_bluetooth_device_ptr_ = nullptr;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      paired_callback_;
  base::MockCallback<
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>>
      account_key_failure_callback_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      pairing_procedure_complete_;
  FakeFastPairRepository fast_pair_repository_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<FastPairGattServiceClient> gatt_service_client_;
  FakeFastPairGattServiceClientImplFactory fast_pair_gatt_service_factory_;
  FakeFastPairDataEncryptor* data_encryptor_ = nullptr;
  std::unique_ptr<FastPairPairer> pairer_;
  base::WeakPtrFactory<FastPairPairerImplTest> weak_ptr_factory_{this};
};

TEST_F(FastPairPairerImplTest, NoPairingIfHandshakeLost) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();
  FailedHandshakeSetUp(Protocol::kFastPairInitial);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
}

TEST_F(FastPairPairerImplTest, FailedCallbackInvokedOnDeviceNotFound) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceInitialFailure();
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Retroactive) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairRetroactive);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest, NoCallbackIsInvokedOnGattSuccess_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest, SuccessfulDecryptedResponsePairFailure_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPairFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
}

TEST_F(FastPairPairerImplTest,
       SuccessfulDecryptedResponsePairFailure_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPairDeviceResult, 0);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetPairFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
  histogram_tester().ExpectTotalCount(kPairDeviceResult, 1);
  histogram_tester().ExpectTotalCount(kPairDeviceErrorReason, 1);
}

TEST_F(FastPairPairerImplTest, SuccessfulDecryptedResponsePairSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest,
       SuccessfulDecryptedResponsePairSuccess_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerImplTest,
       SuccessfulDecryptedResponseConnectFailure_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetConnectFailure();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 1);
}

TEST_F(FastPairPairerImplTest,
       SuccessfulDecryptedResponseConnectFailure_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetConnectFailure();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
  histogram_tester().ExpectTotalCount(kConnectDeviceResult, 1);
}

TEST_F(FastPairPairerImplTest,
       SuccessfulDecryptedResponseConnectSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
}

TEST_F(FastPairPairerImplTest,
       SuccessfulDecryptedResponseConnectSuccess_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyFailure_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();

  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyFailure_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
  histogram_tester().ExpectTotalCount(kWritePasskeyCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWritePasskeyCharacteristicPairFailureMetric, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
}

TEST_F(FastPairPairerImplTest,
       ParseDecryptedPasskeyIncorrectMessageType_Initial_SeekersPasskey) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kSeekersPasskey);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(
    FastPairPairerImplTest,
    ParseDecryptedPasskeyIncorrectMessageType_Initial_KeyBasedPairingRequest) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingRequest);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(
    FastPairPairerImplTest,
    ParseDecryptedPasskeyIncorrectMessageType_Initial_KeyBasedPairingResponse) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingResponse);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyNoPasskey) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForNoPasskey();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyDecryptFailure);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest,
       ParseDecryptedPasskeyIncorrectMessageType_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType(
      FastPairMessageType::kKeyBasedPairingResponse);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyMismatch_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest, ParseDecryptedPasskeyMismatch_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest, PairedDeviceLost_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest, PairedDeviceLost_Subsequent) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Initial_FactoryCreate) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairerAsFactory();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, true);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, PairSuccess_Subsequent_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 0);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kPasskeyCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyAskTime, 1);
  histogram_tester().ExpectTotalCount(kConfirmPasskeyConfirmTime, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, true);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_GuestLoggedIn) {
  Login(user_manager::UserType::USER_TYPE_GUEST);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_KioskAppLoggedIn) {
  Login(user_manager::UserType::USER_TYPE_KIOSK_APP);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_NotLoggedIn) {
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Initial_Locked) {
  GetSessionControllerClient()->LockScreen();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, true);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairRetroactive);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKey_Retroactive_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairRetroactive);
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccountKeyFailure_Initial_GattErrorFailed) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_FAILED);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorUnknown) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_UNKNOWN);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorInProgress) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_IN_PROGRESS);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorInvalidLength) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_INVALID_LENGTH);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotPermitted) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_NOT_PERMITTED);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotAuthorized) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_NOT_AUTHORIZED);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotPaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_NOT_PAIRED);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest,
       WriteAccountKeyFailure_Initial_GattErrorNotSupported) {
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceConnectFailure();
  CreatePairer();
  SetPublicKey();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_NOT_SUPPORTED);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, FastPairVersionOne_DevicePaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/true,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  EXPECT_EQ(GetSystemTrayClient()->show_bluetooth_pairing_dialog_count(), 1);
  EXPECT_CALL(paired_callback_, Run);
  EXPECT_CALL(pairing_procedure_complete_, Run);
  DevicePaired();
}

TEST_F(FastPairPairerImplTest, FastPairVersionOne_DeviceUnpaired) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::RunLoop().RunUntilIdle();

  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/true,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  EXPECT_EQ(GetSystemTrayClient()->show_bluetooth_pairing_dialog_count(), 1);
  EXPECT_CALL(paired_callback_, Run).Times(0);
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(0);
  DeviceUnpaired();
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedOut_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, true);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedIn_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_OptedOut_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

TEST_F(FastPairPairerImplTest, WriteAccount_StatusUnknown_FlagEnabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, true);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairPairerImplTest, WriteAccount_StatusUnknown_FlagDisabled) {
  Login(user_manager::UserType::USER_TYPE_REGULAR);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kFastPairSavedDevices, false);
  fast_pair_repository_.SetOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN);
  base::RunLoop().RunUntilIdle();

  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 0);
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPublicKey();
  SetGetDeviceConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceInitialSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run).Times(1);
  RunWriteAccountKeyCallback();
  histogram_tester().ExpectTotalCount(
      kWriteAccountKeyCharacteristicResultMetric, 1);
}

}  // namespace quick_pair
}  // namespace ash
