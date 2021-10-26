// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"

#include <memory>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/quick_pair/common/account_key_failure.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/fast_pair/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/pairing/fast_pair/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const std::vector<uint8_t> kResponseBytes = {0x01, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
                                             0x32, 0x1D, 0xA0, 0xBA, 0xF0, 0xBB,
                                             0x95, 0x1F, 0xF7, 0xB6};
std::array<uint8_t, 6> kAddressBytes = {12, 14, 76, 200, 5, 8};
std::array<uint8_t, 9> kRequestSaltBytes = {0xF0, 0xBB, 0x95, 0x1F, 0xF7,
                                            0xB6, 0xBA, 0xF0, 0xBB};
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

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    // To mock when we have to pair via address.
    if (get_device_failure_) {
      return nullptr;
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

  void SetGetDeviceFalure() { get_device_failure_ = true; }

  void SetGetDeviceSuccess() { get_device_failure_ = false; }

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
  bool get_device_failure_ = false;
  bool connect_device_failure_ = false;
  device::BluetoothDevice::PairingDelegate* pairing_delegate_ = nullptr;
};

class FakeBluetoothDevice
    : public testing::NiceMock<device::MockBluetoothDevice> {
 public:
  FakeBluetoothDevice(FakeBluetoothAdapter* adapter)
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

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairFakeDataEncryptor : public FastPairDataEncryptor {
 public:
  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) override {
    return encrypted_bytes_;
  }

  const absl::optional<std::array<uint8_t, 64>>& GetPublicKey() override {
    if (public_key_) {
      static absl::optional<std::array<uint8_t, 64>> val = kPublicKey;
      return val;
    }

    static absl::optional<std::array<uint8_t, 64>> val = absl::nullopt;
    return val;
  }

  void SetPublicKey() { public_key_ = true; }

  void ParseDecryptedResponse(
      const std::vector<uint8_t>& encrypted_response_bytes,
      base::OnceCallback<void(const absl::optional<DecryptedResponse>&)>
          callback) override {
    std::move(callback).Run(response_);
  }

  void ParseDecryptedPasskey(
      const std::vector<uint8_t>& encrypted_passkey_bytes,
      base::OnceCallback<void(const absl::optional<DecryptedPasskey>&)>
          callback) override {
    std::move(callback).Run(passkey_);
  }

  FastPairFakeDataEncryptor() = default;
  ~FastPairFakeDataEncryptor() override = default;

  void SetEncryptedBytes(std::array<uint8_t, kBlockSizeBytes> encrypted_bytes) {
    encrypted_bytes_ = std::move(encrypted_bytes);
  }

  void SetDecryptedResponse(const absl::optional<DecryptedResponse> response) {
    response_ = response;
  }

  void SetDecryptedPasskey(const absl::optional<DecryptedPasskey> passkey) {
    passkey_ = passkey;
  }

 private:
  bool public_key_ = false;
  std::array<uint8_t, kBlockSizeBytes> encrypted_bytes_ = {};
  absl::optional<DecryptedResponse> response_ = absl::nullopt;
  absl::optional<DecryptedPasskey> passkey_ = absl::nullopt;
};

class FastPairFakeDataEncryptorImplFactory
    : public FastPairDataEncryptorImpl::Factory {
 public:
  void CreateInstance(
      scoped_refptr<Device> device,
      base::OnceCallback<void(std::unique_ptr<FastPairDataEncryptor>)>
          on_get_instance_callback) override {
    if (!successful_retrieval_) {
      std::move(on_get_instance_callback).Run(nullptr);
      return;
    }

    auto data_encryptor = base::WrapUnique(new FastPairFakeDataEncryptor());
    data_encryptor_ = data_encryptor.get();
    std::move(on_get_instance_callback).Run(std::move(data_encryptor));
  }

  FastPairFakeDataEncryptor* data_encryptor() { return data_encryptor_; }

  ~FastPairFakeDataEncryptorImplFactory() override = default;

  void SetFailedRetrieval() { successful_retrieval_ = false; }

 private:
  FastPairFakeDataEncryptor* data_encryptor_ = nullptr;
  bool successful_retrieval_ = true;
};

class FakeFastPairGattServiceClientImplFactory
    : public FastPairGattServiceClientImpl::Factory {
 public:
  ~FakeFastPairGattServiceClientImplFactory() override = default;

  FakeFastPairGattServiceClient* fake_fast_pair_gatt_service_client() {
    return fake_fast_pair_gatt_service_client_;
  }

 private:
  // FastPairGattServiceClientImpl::Factory:
  std::unique_ptr<FastPairGattServiceClient> CreateInstance(
      device::BluetoothDevice* device,
      scoped_refptr<device::BluetoothAdapter> adapter,
      base::OnceCallback<void(absl::optional<PairFailure>)>
          on_initialized_callback) override {
    auto fake_fast_pair_gatt_service_client =
        std::make_unique<FakeFastPairGattServiceClient>(
            device, adapter, std::move(on_initialized_callback));
    fake_fast_pair_gatt_service_client_ =
        fake_fast_pair_gatt_service_client.get();
    return fake_fast_pair_gatt_service_client;
  }

  FakeFastPairGattServiceClient* fake_fast_pair_gatt_service_client_ = nullptr;
};

class FastPairPairerTest : public AshTestBase {
 public:
  void SuccessfulDataEncryptorSetUp(bool fast_pair_v1 = false) {
    device_ = base::MakeRefCounted<Device>(kMetadataId,
                                           kBluetoothCanonicalizedAddress,
                                           Protocol::kFastPairInitial);

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

    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);

    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &fast_pair_data_encryptor_factory);
  }

  void FailedDataEncryptorSetUp() {
    device_ = base::MakeRefCounted<Device>(kMetadataId,
                                           kBluetoothCanonicalizedAddress,
                                           Protocol::kFastPairInitial);
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

    // Need to add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    fake_bluetooth_device_ =
        std::make_unique<FakeBluetoothDevice>(adapter_.get());
    adapter_->AddMockDevice(std::move(fake_bluetooth_device_));

    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);

    fast_pair_data_encryptor_factory.SetFailedRetrieval();

    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &fast_pair_data_encryptor_factory);
  }

  void RunOnGattClientInitializedCallback(
      absl::optional<PairFailure> failure = absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunOnGattClientInitializedCallback(failure);
  }

  void SetDecryptResponseForIncorrectMessageType() {
    DecryptedResponse response(FastPairMessageType::kSeekersPasskey,
                               kAddressBytes, kRequestSaltBytes);
    fast_pair_data_encryptor_factory.data_encryptor()->SetDecryptedResponse(
        response);
  }

  void SetDecryptPasskeyForIncorrectMessageType() {
    DecryptedPasskey passkey(FastPairMessageType::kKeyBasedPairingResponse,
                             kValidPasskey, kPasskeySaltBytes);
    fast_pair_data_encryptor_factory.data_encryptor()->SetDecryptedPasskey(
        passkey);
  }

  void SetDecryptPasskeyForPasskeyMismatch() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kInvalidPasskey, kPasskeySaltBytes);
    fast_pair_data_encryptor_factory.data_encryptor()->SetDecryptedPasskey(
        passkey);
  }

  void SetDecryptPasskeyForSuccess() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kValidPasskey, kPasskeySaltBytes);
    fast_pair_data_encryptor_factory.data_encryptor()->SetDecryptedPasskey(
        passkey);
  }

  void SetDecryptResponseForSuccess() {
    DecryptedResponse response(FastPairMessageType::kKeyBasedPairingResponse,
                               kAddressBytes, kRequestSaltBytes);
    fast_pair_data_encryptor_factory.data_encryptor()->SetDecryptedResponse(
        response);
  }

  void RunWriteResponseCallback(
      std::vector<uint8_t> data,
      absl::optional<PairFailure> failure = absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWriteResponseCallback(data, failure);
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

  void SetGetDeviceFailure() { adapter_->SetGetDeviceFalure(); }

  void SetConnectFailure() { adapter_->SetConnectFailure(); }

  void SetGetDeviceSuccess() { adapter_->SetGetDeviceSuccess(); }

  bool IsDevicePaired() { return fake_bluetooth_device_ptr_->IsDevicePaired(); }

  bool IsAccountKeySavedToFootprints() {
    return fast_pair_repository_.HasKeyForDevice(
        fake_bluetooth_device_ptr_->GetAddress());
  }

  void SetPublicKey() {
    fast_pair_data_encryptor_factory.data_encryptor()->SetPublicKey();
  }

 protected:
  // This is done on-demand to enable setting up mock expectations first.
  void CreatePairer() {
    pairer_ = std::make_unique<FastPairPairer>(
        adapter_, device_, paired_callback_.Get(),
        base::BindOnce(&FastPairPairerTest::PairFailedCallback,
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
  FakeFastPairGattServiceClientImplFactory fast_pair_gatt_service_factory_;
  FastPairFakeDataEncryptorImplFactory fast_pair_data_encryptor_factory;
  FakeFastPairRepository fast_pair_repository_;
  std::unique_ptr<FastPairPairer> pairer_;
  base::WeakPtrFactory<FastPairPairerTest> weak_ptr_factory_{this};
};

TEST_F(FastPairPairerTest, NoCallbackIsInvokedOnGattSuccess) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, PairFailedCallbackIsInvokedOnGattFailure) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback(PairFailure::kCreateGattConnection);
  EXPECT_EQ(GetPairFailure(), PairFailure::kCreateGattConnection);
}

TEST_F(FastPairPairerTest, PairFailedCallbackWriteResponseFailed) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  RunWriteResponseCallback({}, PairFailure::kKeyBasedPairingResponseTimeout);
  EXPECT_EQ(GetPairFailure(), PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(FastPairPairerTest,
       PairFailedCallbackWriteResponseSuccess) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(),
            PairFailure::kKeybasedPairingResponseDecryptFailure);
}

TEST_F(FastPairPairerTest, PairFailedCallbackIncorrectMessageType) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForIncorrectMessageType();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(),
            PairFailure::kIncorrectKeyBasedPairingResponseType);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponsePairFailure) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetPairFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponsePairSuccess) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponseConnectFailure) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  SetConnectFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponseConnectSuccess) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyFailure) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyIncorrectMessageType) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyMismatch) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
}

TEST_F(FastPairPairerTest, PairedDeviceLost) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
}

TEST_F(FastPairPairerTest, PairSuccess) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
}

TEST_F(FastPairPairerTest, WriteAccountKey) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  SetPublicKey();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_TRUE(IsDevicePaired());
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
  EXPECT_TRUE(IsAccountKeySavedToFootprints());
}

TEST_F(FastPairPairerTest, WriteAccountKeyFailure) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  SetDecryptResponseForSuccess();
  SetGetDeviceFailure();
  SetPublicKey();
  RunWriteResponseCallback(kResponseBytes);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(paired_callback_, Run);
  SetDecryptPasskeyForSuccess();
  SetGetDeviceSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_FAILED);
  EXPECT_FALSE(IsAccountKeySavedToFootprints());
}

TEST_F(FastPairPairerTest, FastPairVersionOne) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/true);
  CreatePairer();
  EXPECT_EQ(GetSystemTrayClient()->show_bluetooth_pairing_dialog_count(), 1);
}

}  // namespace quick_pair
}  // namespace ash
