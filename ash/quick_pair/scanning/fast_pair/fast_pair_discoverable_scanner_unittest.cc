// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/scanning/fast_pair/fast_pair_discoverable_scanner.h"
#include <memory>
#include <vector>
#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/pair_failure.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_data_encryptor.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/scanning/fast_pair/fake_fast_pair_scanner.h"
#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kValidModelId[] = "718c17";
const std::string kAddress = "test_address";
}  // namespace

namespace ash {
namespace quick_pair {

class FastPairDiscoverableScannerTest : public testing::Test {
 public:
  void SetUp() override {
    repository_ = std::make_unique<FakeFastPairRepository>();

    nearby::fastpair::Device metadata;
    metadata.set_trigger_distance(2);
    repository_->SetFakeMetadata(kValidModelId, metadata);

    scanner_ = base::MakeRefCounted<FakeFastPairScanner>();

    adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();

    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());

    data_parser_ = std::make_unique<FastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());

    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_enviornment_.GetMainThreadTaskRunner());

    EXPECT_CALL(*mock_process_manager(), GetProcessReference)
        .WillRepeatedly([&](QuickPairProcessManager::ProcessStoppedCallback) {
          return std::make_unique<
              QuickPairProcessManagerImpl::ProcessReferenceImpl>(
              data_parser_remote_, base::DoNothing());
        });

    FastPairHandshakeLookup::SetCreateFunctionForTesting(
        base::BindRepeating(&FastPairDiscoverableScannerTest::CreateHandshake,
                            base::Unretained(this)));

    discoverable_scanner_ = std::make_unique<FastPairDiscoverableScanner>(
        scanner_, adapter_, found_device_callback_.Get(),
        lost_device_callback_.Get());
  }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

 protected:
  device::BluetoothDevice* GetDevice(const std::string& hex_model_id,
                                     bool is_paired = false) {
    auto device = std::make_unique<device::MockBluetoothDevice>(
        adapter_.get(), 0, "test_name", kAddress, /*paired=*/is_paired,
        /*connected=*/false);

    if (!hex_model_id.empty()) {
      std::vector<uint8_t> model_id_bytes;
      base::HexStringToBytes(hex_model_id, &model_id_bytes);
      device->SetServiceDataForUUID(kFastPairBluetoothUuid, model_id_bytes);
    }

    device::BluetoothDevice* device_ptr = device.get();

    adapter_->AddMockDevice(std::move(device));
    ON_CALL(*adapter_, GetDevice(kAddress))
        .WillByDefault(testing::Return(device_ptr));

    return device_ptr;
  }

  std::unique_ptr<FastPairHandshake> CreateHandshake(
      scoped_refptr<Device> device,
      FastPairHandshake::OnCompleteCallback callback) {
    auto fake = std::make_unique<FakeFastPairHandshake>(
        adapter_, std::move(device), std::move(callback));

    fake_fast_pair_handshake_ = fake.get();

    return fake;
  }

  base::test::SingleThreadTaskEnvironment task_enviornment_;
  scoped_refptr<FakeFastPairScanner> scanner_;
  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<FastPairDiscoverableScanner> discoverable_scanner_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;
  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;
  base::MockCallback<DeviceCallback> found_device_callback_;
  base::MockCallback<DeviceCallback> lost_device_callback_;
  FakeFastPairHandshake* fake_fast_pair_handshake_ = nullptr;
};

TEST_F(FastPairDiscoverableScannerTest, NoServiceData) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  std::unique_ptr<device::BluetoothDevice> device =
      base::WrapUnique(static_cast<device::BluetoothDevice*>(
          new testing::NiceMock<device::MockBluetoothDevice>(
              adapter_.get(), 0, "test_name", "test_address",
              /*paired=*/false,
              /*connected=*/false)));

  scanner_->NotifyDeviceFound(device.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, NoModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice("");
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, ValidModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(1);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, NearbyShareModelId) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice("fc128e");
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, InvokesLostCallbackAfterFound_v1) {
  device::BluetoothDevice* device = GetDevice(kValidModelId);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, InvokesLostCallbackAfterFound_v2) {
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  auto* key_pair = new ::nearby::fastpair::AntiSpoofingKeyPair();
  key_pair->set_public_key("test_public_key");
  metadata.set_allocated_anti_spoofing_key_pair(key_pair);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  device::BluetoothDevice* device = GetDevice(kValidModelId);

  EXPECT_CALL(found_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();

  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(1);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, AlreadyPaired_v1) {
  device::BluetoothDevice* device =
      GetDevice(kValidModelId, /*is_paired=*/true);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, AlreadyPaired_v2) {
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  auto* key_pair = new ::nearby::fastpair::AntiSpoofingKeyPair();
  key_pair->set_public_key("test_public_key");
  metadata.set_allocated_anti_spoofing_key_pair(key_pair);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  device::BluetoothDevice* device =
      GetDevice(kValidModelId, /*is_paired=*/true);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();

  fake_fast_pair_handshake_->InvokeCallback();
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest, HandshakeFailed) {
  nearby::fastpair::Device metadata;
  metadata.set_trigger_distance(2);
  auto* key_pair = new ::nearby::fastpair::AntiSpoofingKeyPair();
  key_pair->set_public_key("test_public_key");
  metadata.set_allocated_anti_spoofing_key_pair(key_pair);
  repository_->SetFakeMetadata(kValidModelId, metadata);

  device::BluetoothDevice* device = GetDevice(kValidModelId);

  EXPECT_CALL(found_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceFound(device);

  base::RunLoop().RunUntilIdle();

  fake_fast_pair_handshake_->InvokeCallback(PairFailure::kCreateGattConnection);
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  scanner_->NotifyDeviceLost(device);

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairDiscoverableScannerTest,
       DoesntInvokeLostCallbackIfDidntInvokeFound) {
  EXPECT_CALL(found_device_callback_, Run).Times(0);
  EXPECT_CALL(lost_device_callback_, Run).Times(0);
  device::BluetoothDevice* device = GetDevice(kValidModelId);
  scanner_->NotifyDeviceLost(device);
  base::RunLoop().RunUntilIdle();
}

}  // namespace quick_pair
}  // namespace ash
