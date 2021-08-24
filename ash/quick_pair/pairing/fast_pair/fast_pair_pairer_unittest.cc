// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/fast_pair/fast_pair_pairer.h"

#include <memory>

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
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

const std::vector<uint8_t> kResponseBytes = {0xCF, 0x5E, 0x3F, 0x45, 0x61, 0xC3,
                                             0x32, 0x1D, 0xA0, 0xBA, 0xF0, 0xBB,
                                             0x95, 0x1F, 0xF7, 0xB6};

constexpr char kMetadataId[] = "test_metadata_id";
constexpr char kAddress[] = "testad";
constexpr char kDeviceName[] = "test_device_name";

class FakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  FakeBluetoothAdapter() = default;

  // Move-only class
  FakeBluetoothAdapter(const FakeBluetoothAdapter&) = delete;
  FakeBluetoothAdapter& operator=(const FakeBluetoothAdapter&) = delete;

  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void NotifyGattDiscoveryCompleteForService(
      device::BluetoothRemoteGattService* service) {
    device::BluetoothAdapter::NotifyGattDiscoveryComplete(service);
  }

 protected:
  ~FakeBluetoothAdapter() override = default;
};

}  // namespace

namespace ash {
namespace quick_pair {

class FakeFastPairDataEncryptor : public FastPairDataEncryptor {
 public:
  const std::array<uint8_t, kBlockSizeBytes> EncryptBytes(
      const std::array<uint8_t, kBlockSizeBytes>& bytes_to_encrypt) override {
    return encrypted_bytes_;
  }

  FakeFastPairDataEncryptor() = default;
  ~FakeFastPairDataEncryptor() override = default;

  void SetEncryptedBytes(std::array<uint8_t, kBlockSizeBytes> encrypted_bytes) {
    encrypted_bytes_ = std::move(encrypted_bytes);
  }

 private:
  std::array<uint8_t, kBlockSizeBytes> encrypted_bytes_ = {};
};

class FakeFastPairDataEncryptorImplFactory
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

    std::unique_ptr<FastPairDataEncryptor> data_encryptor =
        base::WrapUnique(new FakeFastPairDataEncryptor());
    std::move(on_get_instance_callback).Run(std::move(data_encryptor));
  }

  ~FakeFastPairDataEncryptorImplFactory() override = default;

  void SetFailedRetrieval() { successful_retrieval_ = false; }

 private:
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

class FastPairPairerTest : public testing::Test {
 public:
  void SuccessfulDataEncryptorSetUp() {
    device_ = base::MakeRefCounted<Device>(kMetadataId, kAddress,
                                           Protocol::kFastPair);
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

    // Need to add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), 0, kDeviceName, kAddress, /*paired=*/true,
            /*connected*/ false);
    adapter_->AddMockDevice(std::move(mock_device_));

    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &fast_pair_gatt_service_factory_);

    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &fast_pair_data_encryptor_factory);
  }

  void FailedDataEncryptorSetUp() {
    device_ = base::MakeRefCounted<Device>(kMetadataId, kAddress,
                                           Protocol::kFastPair);
    adapter_ = base::MakeRefCounted<FakeBluetoothAdapter>();

    // Need to add a matching mock device to the bluetooth adapter with the
    // same address to mock the relationship between Device and
    // device::BluetoothDevice.
    mock_device_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            adapter_.get(), 0, kDeviceName, kAddress, /*paired=*/true,
            /*connected*/ false);
    adapter_->AddMockDevice(std::move(mock_device_));

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

  void RunWriteResponseCallback(
      std::vector<uint8_t> data,
      absl::optional<PairFailure> failure = absl::nullopt) {
    fast_pair_gatt_service_factory_.fake_fast_pair_gatt_service_client()
        ->RunWriteResponseCallback(data, failure);
  }

 protected:
  // This is done on-demand to enable setting up mock expectations first.
  void CreatePairer() {
    pairer_ = std::make_unique<FastPairPairer>(
        adapter_, device_, paired_callback_.Get(), pair_failed_callback_.Get(),
        account_key_failure_callback_.Get(), pairing_procedure_complete_.Get());
  }

  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>> mock_device_;
  scoped_refptr<FakeBluetoothAdapter> adapter_;
  scoped_refptr<Device> device_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      paired_callback_;
  base::MockCallback<
      base::OnceCallback<void(scoped_refptr<Device>, PairFailure)>>
      pair_failed_callback_;
  base::MockCallback<
      base::OnceCallback<void(scoped_refptr<Device>, AccountKeyFailure)>>
      account_key_failure_callback_;
  base::MockCallback<base::OnceCallback<void(scoped_refptr<Device>)>>
      pairing_procedure_complete_;
  FakeFastPairGattServiceClientImplFactory fast_pair_gatt_service_factory_;
  FakeFastPairDataEncryptorImplFactory fast_pair_data_encryptor_factory;
  std::unique_ptr<FastPairPairer> pairer_;
};

TEST_F(FastPairPairerTest, NoCallbackIsInvokedOnGattSuccess) {
  SuccessfulDataEncryptorSetUp();
  EXPECT_CALL(pair_failed_callback_, Run).Times(0);
  CreatePairer();
  RunOnGattClientInitializedCallback();
}

TEST_F(FastPairPairerTest, PairFailedCallbackIsInvokedOnGattFailure) {
  SuccessfulDataEncryptorSetUp();
  EXPECT_CALL(pair_failed_callback_, Run);
  CreatePairer();
  RunOnGattClientInitializedCallback(PairFailure::kCreateGattConnection);
}

TEST_F(FastPairPairerTest, PairFailedCallbackWriteResponseFailed) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  EXPECT_CALL(pair_failed_callback_, Run);
  RunWriteResponseCallback({}, PairFailure::kKeyBasedPairingResponseTimeout);
}

TEST_F(FastPairPairerTest,
       PairFailedCallbackWriteResponseSuccess) {
  SuccessfulDataEncryptorSetUp();
  CreatePairer();
  RunOnGattClientInitializedCallback();
  EXPECT_CALL(pair_failed_callback_, Run).Times(0);
  RunWriteResponseCallback(kResponseBytes);
}

}  // namespace quick_pair
}  // namespace ash
