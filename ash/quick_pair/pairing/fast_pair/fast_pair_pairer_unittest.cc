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
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/services/quick_pair/public/cpp/decrypted_passkey.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
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

}  // namespace

namespace ash {
namespace quick_pair {

class FastPairPairerTest : public AshTestBase {
 public:
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

    gatt_service_client_ = new FakeFastPairGattServiceClient(
        fake_bluetooth_device_ptr_, adapter_, base::DoNothing());

    data_encryptor_ = new FakeFastPairDataEncryptor();

    FastPairHandshakeLookup::SetCreateFunctionForTesting(base::BindRepeating(
        &FastPairPairerTest::CreateHandshake, base::Unretained(this)));
    FastPairHandshakeLookup::GetInstance()->Create(adapter_, device_,
                                                   base::DoNothing());
  }

  std::unique_ptr<FastPairHandshake> CreateHandshake(
      scoped_refptr<Device> device,
      FastPairHandshake::OnCompleteCallback callback) {
    auto fake = std::make_unique<FakeFastPairHandshake>(
        adapter_, device, std::move(callback),
        base::WrapUnique(data_encryptor_),
        base::WrapUnique(gatt_service_client_));

    fake->InvokeCallback();

    return fake;
  }

  void SetDecryptResponseForIncorrectMessageType() {
    DecryptedResponse response(FastPairMessageType::kSeekersPasskey,
                               kAddressBytes, kRequestSaltBytes);
    data_encryptor_->response(response);
  }

  void SetDecryptPasskeyForIncorrectMessageType() {
    DecryptedPasskey passkey(FastPairMessageType::kKeyBasedPairingResponse,
                             kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void SetDecryptPasskeyForPasskeyMismatch() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kInvalidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void SetDecryptPasskeyForSuccess() {
    DecryptedPasskey passkey(FastPairMessageType::kProvidersPasskey,
                             kValidPasskey, kPasskeySaltBytes);
    data_encryptor_->passkey(passkey);
  }

  void RunWritePasskeyCallback(
      std::vector<uint8_t> data,
      absl::optional<PairFailure> failure = absl::nullopt) {
    gatt_service_client_->RunWritePasskeyCallback(data, failure);
  }

  void RunWriteAccountKeyCallback(
      absl::optional<device::BluetoothGattService::GattErrorCode> error =
          absl::nullopt) {
    gatt_service_client_->RunWriteAccountKeyCallback(error);
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

  void SetPublicKey() { data_encryptor_->public_key(kPublicKey); }

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
  FakeFastPairRepository fast_pair_repository_;
  std::unique_ptr<FastPairPairer> pairer_;

  FakeFastPairGattServiceClient* gatt_service_client_ = nullptr;
  FakeFastPairDataEncryptor* data_encryptor_ = nullptr;

  base::WeakPtrFactory<FastPairPairerTest> weak_ptr_factory_{this};
};

TEST_F(FastPairPairerTest, NoCallbackIsInvokedOnGattSuccess_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, NoCallbackIsInvokedOnGattSuccess_Retroactive) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairRetroactive);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, NoCallbackIsInvokedOnGattSuccess_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponsePairFailure_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetPairFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponsePairFailure_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetPairFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingConnect);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponsePairSuccess_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponsePairSuccess_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponseConnectFailure_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  SetConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
}

TEST_F(FastPairPairerTest,
       SuccessfulDecryptedResponseConnectFailure_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  SetConnectFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), PairFailure::kAddressConnect);
}

TEST_F(FastPairPairerTest, SuccessfulDecryptedResponseConnectSuccess_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest,
       SuccessfulDecryptedResponseConnectSuccess_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyFailure_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyFailure_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback({}, PairFailure::kPasskeyPairingCharacteristicWrite);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyPairingCharacteristicWrite);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyIncorrectMessageType_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
}

TEST_F(FastPairPairerTest,
       ParseDecryptedPasskeyIncorrectMessageType_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForIncorrectMessageType();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kIncorrectPasskeyResponseType);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyMismatch_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
}

TEST_F(FastPairPairerTest, ParseDecryptedPasskeyMismatch_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForPasskeyMismatch();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPasskeyMismatch);
}

TEST_F(FastPairPairerTest, PairedDeviceLost_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
}

TEST_F(FastPairPairerTest, PairedDeviceLost_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetPairFailure(), absl::nullopt);
  SetDecryptPasskeyForSuccess();
  NotifyConfirmPasskey();
  base::RunLoop().RunUntilIdle();
  RunWritePasskeyCallback(kResponseBytes);
  EXPECT_EQ(GetPairFailure(), PairFailure::kPairingDeviceLost);
}

TEST_F(FastPairPairerTest, PairSuccess_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
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

TEST_F(FastPairPairerTest, PairSuccess_Subsequent) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairSubsequent);
  SetGetDeviceFailure();
  CreatePairer();
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

TEST_F(FastPairPairerTest, WriteAccountKey_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  SetPublicKey();
  CreatePairer();
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

TEST_F(FastPairPairerTest, WriteAccountKey_Retroactive) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairRetroactive);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(pairing_procedure_complete_, Run);
  RunWriteAccountKeyCallback();
}

TEST_F(FastPairPairerTest, WriteAccountKeyFailure_Initial) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairInitial);
  SetGetDeviceFailure();
  CreatePairer();
  SetPublicKey();
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
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/true,
                               /*protocol=*/Protocol::kFastPairInitial);
  CreatePairer();
  EXPECT_EQ(GetSystemTrayClient()->show_bluetooth_pairing_dialog_count(), 1);
}

TEST_F(FastPairPairerTest, WriteAccountKeyFailure_Retroactive) {
  SuccessfulDataEncryptorSetUp(/*fast_pair_v1=*/false,
                               /*protocol=*/Protocol::kFastPairRetroactive);
  SetGetDeviceFailure();
  CreatePairer();
  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(account_key_failure_callback_, Run);
  RunWriteAccountKeyCallback(
      device::BluetoothGattService::GattErrorCode::GATT_ERROR_FAILED);
}

}  // namespace quick_pair
}  // namespace ash
