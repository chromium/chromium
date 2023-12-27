// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/async_fast_pair_handshake_lookup_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"

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

const std::string kMetadataId = "test_id";
const std::string kAddress = "test_address";

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
      base::OnceCallback<void(std::optional<PairFailure>)>
          on_initialized_callback) override {
    auto fake_fast_pair_gatt_service_client =
        std::make_unique<FakeFastPairGattServiceClient>(
            device, adapter, std::move(on_initialized_callback));
    fake_fast_pair_gatt_service_client_ =
        fake_fast_pair_gatt_service_client.get();
    return fake_fast_pair_gatt_service_client;
  }

  raw_ptr<FakeFastPairGattServiceClient, DanglingUntriaged>
      fake_fast_pair_gatt_service_client_ = nullptr;
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
  raw_ptr<FakeFastPairDataEncryptor, DanglingUntriaged> data_encryptor_ =
      nullptr;
  bool successful_retrieval_ = true;
};

}  // namespace

namespace ash::quick_pair {

class AsyncFastPairHandshakeLookupImplTest : public testing::Test {
 public:
  void SetUp() override {
    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &gatt_service_client_factory_);

    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &data_encryptor_factory_);

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    device_ = base::MakeRefCounted<Device>(kMetadataId, kAddress,
                                           Protocol::kFastPairInitial);

    mock_device_ = std::make_unique<device::MockBluetoothDevice>(
        adapter_.get(), /*bluetooth_class=*/0, "test_device_name", kAddress,
        /*paired=*/false, /*connected=*/false);
    ON_CALL(*(adapter_.get()), GetDevice(kAddress))
        .WillByDefault(testing::Return(mock_device_.get()));
  }

  void TearDown() override {
    AsyncFastPairHandshakeLookupImpl::GetAsyncInstance()->Clear();
    is_complete_ = false;
  }

 protected:
  void CreateHandshake() {
    AsyncFastPairHandshakeLookupImpl::GetAsyncInstance()->Create(
        adapter_, device_,
        base::BindOnce(
            &AsyncFastPairHandshakeLookupImplTest::OnCompleteCallback,
            weak_pointer_factory_.GetWeakPtr()));
  }

  FastPairHandshake* GetHandshake() {
    return AsyncFastPairHandshakeLookupImpl::GetAsyncInstance()->Get(device_);
  }

  void ExpectOnCompleteCalled() { EXPECT_TRUE(is_complete_); }

  FakeFastPairGattServiceClient* fake_fast_pair_gatt_service_client() {
    return gatt_service_client_factory_.fake_fast_pair_gatt_service_client();
  }

  FakeFastPairDataEncryptor* data_encryptor() {
    return data_encryptor_factory_.data_encryptor();
  }

  // The handshake setup has async calls in it, so to finish setting up the
  // handshake, the test needs to manually have the GATT service client and the
  // data encryptor move the process along.
  void RunHandshakeSetupCallbacksNoFailures() {
    fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();

    data_encryptor()->response(std::make_optional(DecryptedResponse(
        FastPairMessageType::kKeyBasedPairingResponse,
        std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
        std::array<uint8_t, kDecryptedResponseSaltByteSize>())));

    fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
        std::vector<uint8_t>());
  }

  FakeFastPairGattServiceClientImplFactory gatt_service_client_factory_;
  FastPairFakeDataEncryptorImplFactory data_encryptor_factory_;

 protected:
  void OnCompleteCallback(scoped_refptr<Device> device,
                          std::optional<PairFailure> failure) {
    EXPECT_EQ(device, device_);
    EXPECT_FALSE(is_complete_);
    is_complete_ = true;
    failure_ = failure;
  }

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;
  scoped_refptr<Device> device_;
  bool is_complete_ = false;
  std::optional<PairFailure> failure_;

  base::WeakPtrFactory<AsyncFastPairHandshakeLookupImplTest>
      weak_pointer_factory_{this};
};

TEST_F(AsyncFastPairHandshakeLookupImplTest,
       CreateAndSuccessfullyCompleteHandshake) {
  CreateHandshake();
  auto* handshake = GetHandshake();
  EXPECT_TRUE(handshake);
  RunHandshakeSetupCallbacksNoFailures();
  ExpectOnCompleteCalled();
  EXPECT_FALSE(failure_.has_value());
}

TEST_F(AsyncFastPairHandshakeLookupImplTest,
       FailThenSuccessfullyCompleteHandshake) {
  CreateHandshake();
  auto* handshake = GetHandshake();
  EXPECT_TRUE(handshake);

  // Inject a test failure during the GATT connection, this results in a
  // handshake failure.
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback(
      PairFailure::kCreateGattConnection);

  // Expect to be on the second attempt after the first attempt failed.
  EXPECT_EQ(2, AsyncFastPairHandshakeLookupImpl::GetAsyncInstance()
                   ->fast_pair_handshake_attempt_counts_[device_]);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();

  data_encryptor()->response(std::make_optional(DecryptedResponse(
      FastPairMessageType::kKeyBasedPairingResponse,
      std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
      std::array<uint8_t, kDecryptedResponseSaltByteSize>())));

  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  ExpectOnCompleteCalled();
  EXPECT_FALSE(failure_.has_value());
}

TEST_F(AsyncFastPairHandshakeLookupImplTest, FailToCreateHandshake) {
  CreateHandshake();
  auto* handshake = GetHandshake();
  EXPECT_TRUE(handshake);

  // Inject a test failure during the GATT connection, each GATT failure results
  // in one handshake failure.
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback(
      PairFailure::kCreateGattConnection);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback(
      PairFailure::kCreateGattConnection);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback(
      PairFailure::kCreateGattConnection);

  // After 3 failures, the OnCompleteCallback will be called with the last
  // failure condition.
  ExpectOnCompleteCalled();
  EXPECT_TRUE(failure_.has_value());
  EXPECT_EQ(failure_.value(), PairFailure::kCreateGattConnection);
}

}  // namespace ash::quick_pair
