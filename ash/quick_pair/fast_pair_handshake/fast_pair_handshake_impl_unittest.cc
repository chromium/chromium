// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl.h"

#include <memory>

#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake.h"
#include "ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using Device = ash::quick_pair::Device;
using FakeFastPairDataEncryptor = ash::quick_pair::FakeFastPairDataEncryptor;
using FakeFastPairGattServiceClient =
    ash::quick_pair::FakeFastPairGattServiceClient;
using FastPairDataEncryptor = ash::quick_pair::FastPairDataEncryptor;
using FastPairDataEncryptorImpl = ash::quick_pair::FastPairDataEncryptorImpl;
using FastPairGattServiceClient = ash::quick_pair::FastPairGattServiceClient;
using FastPairGattServiceClientImpl =
    ash::quick_pair::FastPairGattServiceClientImpl;
using PairFailure = ash::quick_pair::PairFailure;

class FakeFastPairGattServiceClientImplFactory
    : public FastPairGattServiceClientImpl::Factory {
 public:
  ~FakeFastPairGattServiceClientImplFactory() override = default;

  FakeFastPairGattServiceClient* fake_fast_pair_gatt_service_client() {
    return fake_fast_pair_gatt_service_client_;
  }

 private:
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

    auto data_encryptor = base::WrapUnique(new FakeFastPairDataEncryptor());
    data_encryptor_ = data_encryptor.get();
    std::move(on_get_instance_callback).Run(std::move(data_encryptor));
  }

  FakeFastPairDataEncryptor* data_encryptor() { return data_encryptor_; }

  ~FastPairFakeDataEncryptorImplFactory() override = default;

  void SetFailedRetrieval() { successful_retrieval_ = false; }

 private:
  FakeFastPairDataEncryptor* data_encryptor_ = nullptr;
  bool successful_retrieval_ = true;
};

}  // namespace

namespace ash {
namespace quick_pair {

const std::string kMetadataId = "test_id";
const std::string kAddress = "test_address";

class FastPairHandshakeImplTest : public testing::Test {
 public:
  void SetUp() override {
    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    device_ = base::MakeRefCounted<Device>(kMetadataId, kAddress,
                                           Protocol::kFastPairInitial);

    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &gatt_service_client_factory_);

    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &data_encryptor_factory_);

    handshake_ = std::make_unique<FastPairHandshakeImpl>(
        adapter_, device_,
        base::BindLambdaForTesting([this](scoped_refptr<Device> device,
                                          absl::optional<PairFailure> failure) {
          EXPECT_EQ(device_, device);
          failure_ = failure;
        }));
  }

 protected:
  FakeFastPairGattServiceClient* fake_fast_pair_gatt_service_client() {
    return gatt_service_client_factory_.fake_fast_pair_gatt_service_client();
  }

  FakeFastPairDataEncryptor* data_encryptor() {
    return data_encryptor_factory_.data_encryptor();
  }

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  scoped_refptr<Device> device_;
  FakeFastPairGattServiceClientImplFactory gatt_service_client_factory_;
  FastPairFakeDataEncryptorImplFactory data_encryptor_factory_;
  std::unique_ptr<FastPairHandshake> handshake_;
  absl::optional<PairFailure> failure_ = absl::nullopt;
};

TEST_F(FastPairHandshakeImplTest, GattError) {
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback(
      PairFailure::kCreateGattConnection);
  EXPECT_EQ(failure_.value(), PairFailure::kCreateGattConnection);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, DataEncryptorCreateError) {
  data_encryptor_factory_.SetFailedRetrieval();
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  EXPECT_EQ(failure_.value(), PairFailure::kDataEncryptorRetrieval);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, WriteResponseError) {
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>(), PairFailure::kKeyBasedPairingCharacteristicWrite);
  EXPECT_EQ(failure_.value(), PairFailure::kKeyBasedPairingCharacteristicWrite);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, ParseResponseError) {
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  data_encryptor()->response(absl::nullopt);
  EXPECT_EQ(failure_.value(),
            PairFailure::kKeybasedPairingResponseDecryptFailure);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, ParseResponseWrongType) {
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  data_encryptor()->response(absl::make_optional(DecryptedResponse(
      FastPairMessageType::kProvidersPasskey,
      std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
      std::array<uint8_t, kDecryptedResponseSaltByteSize>())));
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  EXPECT_EQ(failure_.value(),
            PairFailure::kIncorrectKeyBasedPairingResponseType);
  EXPECT_FALSE(handshake_->completed_successfully());
}

TEST_F(FastPairHandshakeImplTest, Success) {
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  data_encryptor()->response(absl::make_optional(DecryptedResponse(
      FastPairMessageType::kKeyBasedPairingResponse,
      std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
      std::array<uint8_t, kDecryptedResponseSaltByteSize>())));
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  EXPECT_FALSE(failure_.has_value());
  EXPECT_TRUE(handshake_->completed_successfully());
}

}  // namespace quick_pair
}  // namespace ash
