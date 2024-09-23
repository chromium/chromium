// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_impl_new.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_impl.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/services/quick_pair/public/cpp/decrypted_response.h"
#include "chromeos/ash/services/quick_pair/public/cpp/fast_pair_message_type.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/floss/floss_features.h"
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

const char kDataEncryptorCreateResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.FastPairDataEncryptor.CreateResult";
const char kWriteKeyBasedCharacteristicResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.Result";
const char kWriteKeyBasedCharacteristicPairFailureMetric[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.PairFailure";
const char kKeyBasedCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.DecryptTime";
const char kKeyBasedCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.DecryptResult";
const char kTotalDataEncryptorCreateTimeMetric[] =
    "Bluetooth.ChromeOS.FastPair.FastPairDataEncryptor.CreateTime";
const char kHandshakeResult[] = "Bluetooth.ChromeOS.FastPair.Handshake.Result";
const char kHandshakeFailureReason[] =
    "Bluetooth.ChromeOS.FastPair.Handshake.FailureReason";

}  // namespace

namespace ash {
namespace quick_pair {

const std::string kMetadataId = "test_id";
const std::string kAddress = "test_address";

class FastPairHandshakeImplNewTest : public testing::Test {
 public:
  void SetUp() override {
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

    FastPairGattServiceClientImpl::Factory::SetFactoryForTesting(
        &gatt_service_client_factory_);

    FastPairDataEncryptorImpl::Factory::SetFactoryForTesting(
        &data_encryptor_factory_);

    handshake_ = std::make_unique<FastPairHandshakeImplNew>(adapter_, device_);

    handshake_->SetUpHandshake(
        base::BindLambdaForTesting(
            [this](std::optional<PairFailure> failure) { failure_ = failure; }),
        base::BindLambdaForTesting([this](scoped_refptr<Device> device) {
          EXPECT_EQ(device_, device);
        }));
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  FakeFastPairGattServiceClient* fake_fast_pair_gatt_service_client() {
    return gatt_service_client_factory_.fake_fast_pair_gatt_service_client();
  }

  FakeFastPairDataEncryptor* data_encryptor() {
    return data_encryptor_factory_.data_encryptor();
  }

  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<device::MockBluetoothDevice> mock_device_;
  base::HistogramTester histogram_tester_;
  scoped_refptr<Device> device_;
  FakeFastPairGattServiceClientImplFactory gatt_service_client_factory_;
  FastPairFakeDataEncryptorImplFactory data_encryptor_factory_;
  std::unique_ptr<FastPairHandshake> handshake_;
  std::optional<PairFailure> failure_ = std::nullopt;
};

TEST_F(FastPairHandshakeImplNewTest, GattError) {
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback(
      PairFailure::kCreateGattConnection);
  EXPECT_EQ(failure_.value(), PairFailure::kCreateGattConnection);
  EXPECT_FALSE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 1);
}

TEST_F(FastPairHandshakeImplNewTest, DataEncryptorCreateError) {
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(kDataEncryptorCreateResultMetric, 0);
  histogram_tester().ExpectTotalCount(
      kWriteKeyBasedCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kTotalDataEncryptorCreateTimeMetric, 0);
  data_encryptor_factory_.SetFailedRetrieval();
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  EXPECT_EQ(failure_.value(), PairFailure::kDataEncryptorRetrieval);
  EXPECT_FALSE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(kDataEncryptorCreateResultMetric, 1);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWriteKeyBasedCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kTotalDataEncryptorCreateTimeMetric, 0);
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 1);
}

TEST_F(FastPairHandshakeImplNewTest, WriteResponseError) {
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  histogram_tester().ExpectTotalCount(kDataEncryptorCreateResultMetric, 0);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicResultMetric,
                                      0);
  histogram_tester().ExpectTotalCount(
      kWriteKeyBasedCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kTotalDataEncryptorCreateTimeMetric, 0);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>(), PairFailure::kKeyBasedPairingCharacteristicWrite);
  EXPECT_EQ(failure_.value(), PairFailure::kKeyBasedPairingCharacteristicWrite);
  EXPECT_FALSE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(kDataEncryptorCreateResultMetric, 1);
  histogram_tester().ExpectTotalCount(kTotalDataEncryptorCreateTimeMetric, 1);
  histogram_tester().ExpectTotalCount(kWriteKeyBasedCharacteristicResultMetric,
                                      1);
  histogram_tester().ExpectTotalCount(
      kWriteKeyBasedCharacteristicPairFailureMetric, 1);
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 1);
}

TEST_F(FastPairHandshakeImplNewTest, ParseResponseError) {
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 0);
  histogram_tester().ExpectTotalCount(
      kWriteKeyBasedCharacteristicPairFailureMetric, 0);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  data_encryptor()->response(std::nullopt);
  EXPECT_EQ(failure_.value(),
            PairFailure::kKeybasedPairingResponseDecryptFailure);
  EXPECT_FALSE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(
      kWriteKeyBasedCharacteristicPairFailureMetric, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 1);
}

TEST_F(FastPairHandshakeImplNewTest, ParseResponseWrongType) {
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 0);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  data_encryptor()->response(std::make_optional(DecryptedResponse(
      FastPairMessageType::kKeyBasedPairingExtendedResponse,
      std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
      std::array<uint8_t, kDecryptedResponseSaltByteSize>())));
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  EXPECT_EQ(failure_.value(),
            PairFailure::kIncorrectKeyBasedPairingResponseType);
  EXPECT_FALSE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 1);
}

TEST_F(FastPairHandshakeImplNewTest, Success) {
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 0);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  data_encryptor()->response(std::make_optional(DecryptedResponse(
      FastPairMessageType::kKeyBasedPairingResponse,
      std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
      std::array<uint8_t, kDecryptedResponseSaltByteSize>())));
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  EXPECT_FALSE(failure_.has_value());
  EXPECT_TRUE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
}

TEST_F(FastPairHandshakeImplNewTest, SuccessWithExtendedResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kFastPairKeyboards,
                            floss::features::kFlossEnabled},
      /*disabled_features=*/{});
  histogram_tester().ExpectTotalCount(kHandshakeResult, 0);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 0);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 0);
  fake_fast_pair_gatt_service_client()->RunOnGattClientInitializedCallback();
  data_encryptor()->response(std::make_optional(DecryptedResponse(
      FastPairMessageType::kKeyBasedPairingExtendedResponse,
      std::array<uint8_t, kDecryptedResponseAddressByteSize>(),
      std::array<uint8_t, kDecryptedResponseSaltByteSize>())));
  fake_fast_pair_gatt_service_client()->RunWriteResponseCallback(
      std::vector<uint8_t>());
  EXPECT_FALSE(failure_.has_value());
  EXPECT_TRUE(handshake_->completed_successfully());
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptTime, 1);
  histogram_tester().ExpectTotalCount(kKeyBasedCharacteristicDecryptResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeResult, 1);
  histogram_tester().ExpectTotalCount(kHandshakeFailureReason, 0);
}

TEST_F(FastPairHandshakeImplNewTest, FailsIfNoDevice) {
  auto device = base::MakeRefCounted<Device>(kMetadataId, "invalid_address",
                                             Protocol::kFastPairInitial);

  auto handshake = std::make_unique<FastPairHandshakeImplNew>(adapter_, device);

  handshake->SetUpHandshake(
      base::BindLambdaForTesting(
          [this](std::optional<PairFailure> failure) { failure_ = failure; }),
      base::BindLambdaForTesting([this](scoped_refptr<Device> device) {
        EXPECT_EQ(device_, device);
      }));

  EXPECT_EQ(failure_, PairFailure::kPairingDeviceLost);
}

}  // namespace quick_pair
}  // namespace ash
