// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/pairing/retroactive_pairing_detector.h"

#include <memory>

#include "ash/quick_pair/common/constants.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/pairing/mock_pairer_broker.h"
#include "ash/quick_pair/pairing/pairer_broker.h"
#include "ash/quick_pair/pairing/retroactive_pairing_detector_impl.h"
#include "ash/quick_pair/repository/fake_fast_pair_repository.h"
#include "ash/quick_pair/repository/fast_pair/pairing_metadata.h"
#include "ash/services/quick_pair/fast_pair_data_parser.h"
#include "ash/services/quick_pair/mock_quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process.h"
#include "ash/services/quick_pair/quick_pair_process_manager.h"
#include "ash/services/quick_pair/quick_pair_process_manager_impl.h"
#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr char kValidModelId[] = "718c17";
constexpr char kTestDeviceAddress1[] = "11:12:13:14:15:16";
constexpr char kTestDeviceAddress2[] = "18:12:13:14:15:16";
constexpr char kTestBleDeviceName[] = "Test Device Name";
const char kPublicAntiSpoof[] =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
constexpr long kModelIdLong = 7441431;

std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
CreateTestBluetoothDevice(std::string address) {
  return std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
      /*adapter=*/nullptr, /*bluetooth_class=*/0, kTestBleDeviceName, address,
      /*paired=*/true, /*connected=*/false);
}

}  // namespace

namespace ash {
namespace quick_pair {

class RetroactivePairingDetectorFakeBluetoothAdapter
    : public testing::NiceMock<device::MockBluetoothAdapter> {
 public:
  device::BluetoothDevice* GetDevice(const std::string& address) override {
    for (const auto& it : mock_devices_) {
      if (it->GetAddress() == address)
        return it.get();
    }
    return nullptr;
  }

  void NotifyDevicePairedChanged(device::BluetoothDevice* device,
                                 bool new_paired_status) {
    device::BluetoothAdapter::NotifyDevicePairedChanged(device,
                                                        new_paired_status);
  }

 private:
  ~RetroactivePairingDetectorFakeBluetoothAdapter() = default;
};

class FakeFastPairDataParser : public FastPairDataParser {
 public:
  explicit FakeFastPairDataParser(
      mojo::PendingReceiver<mojom::FastPairDataParser> receiver)
      : FastPairDataParser(std::move(receiver)) {}

  void GetHexModelIdFromServiceData(
      const std::vector<uint8_t>& service_data,
      GetHexModelIdFromServiceDataCallback callback) override {
    std::move(callback).Run(model_id_);
  }

  void SetHexModelIdFromServiceData(absl::optional<std::string> model_id) {
    model_id_ = model_id;
  }

  void ParseNotDiscoverableAdvertisement(
      const std::vector<uint8_t>& service_data,
      ParseNotDiscoverableAdvertisementCallback callback) override {
    std::move(callback).Run(advertisement_);
  }

  void SetParseNotDiscoverableAdvertisement(
      absl::optional<NotDiscoverableAdvertisement>& advertisement) {
    advertisement_ = advertisement;
  }

 private:
  absl::optional<std::string> model_id_;
  absl::optional<NotDiscoverableAdvertisement> advertisement_ = absl::nullopt;
};

class RetroactivePairingDetectorTest
    : public testing::Test,
      public RetroactivePairingDetector::Observer {
 public:
  void SetUp() override {
    adapter_ =
        base::MakeRefCounted<RetroactivePairingDetectorFakeBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    pairer_broker_ = std::make_unique<MockPairerBroker>();
    mock_pairer_broker_ = static_cast<MockPairerBroker*>(pairer_broker_.get());

    repository_ = std::make_unique<FakeFastPairRepository>();

    process_manager_ = std::make_unique<MockQuickPairProcessManager>();
    quick_pair_process::SetProcessManager(process_manager_.get());

    data_parser_ = std::make_unique<FakeFastPairDataParser>(
        fast_pair_data_parser_.InitWithNewPipeAndPassReceiver());
    fake_data_parser_ =
        static_cast<FakeFastPairDataParser*>(data_parser_.get());
    data_parser_remote_.Bind(std::move(fast_pair_data_parser_),
                             task_environment_.GetMainThreadTaskRunner());

    EXPECT_CALL(*mock_process_manager(), GetProcessReference)
        .WillRepeatedly([&](QuickPairProcessManager::ProcessStoppedCallback) {
          return std::make_unique<
              QuickPairProcessManagerImpl::ProcessReferenceImpl>(
              data_parser_remote_, base::DoNothing());
        });

    retroactive_pairing_detector_ =
        std::make_unique<RetroactivePairingDetectorImpl>(pairer_broker_.get());
    retroactive_pairing_detector_->AddObserver(this);
  }

  void SetParseAdvertisementResult(bool show_ui) {
    advertisement_ = NotDiscoverableAdvertisement(
        std::vector<uint8_t>(), /*show_ui=*/show_ui, /*salt=*/1,
        /*battery_notification=*/absl::nullopt);
    fake_data_parser_->SetParseNotDiscoverableAdvertisement(advertisement_);
  }

  MockQuickPairProcessManager* mock_process_manager() {
    return static_cast<MockQuickPairProcessManager*>(process_manager_.get());
  }

  void PairFastPairDeviceWithFastPair(std::string address) {
    fake_data_parser_->SetHexModelIdFromServiceData(kValidModelId);
    auto fp_device = base::MakeRefCounted<Device>(kValidModelId, address,
                                                  Protocol::kFastPairInitial);
    mock_pairer_broker_->NotifyDevicePaired(fp_device);
  }

  void PairFastPairDeviceWithClassicBluetooth(
      bool new_paired_status,
      std::string address,
      bool set_public_key,
      absl::optional<std::string> model_id) {
    fake_data_parser_->SetHexModelIdFromServiceData(model_id);
    if (set_public_key) {
      nearby::fastpair::Device metadata;

      std::string decoded_key;
      base::Base64Decode(kPublicAntiSpoof, &decoded_key);
      metadata.mutable_anti_spoofing_key_pair()->set_public_key(decoded_key);
      repository_->SetFakeMetadata(model_id.value(), metadata);
    }

    bt_device_ = CreateTestBluetoothDevice(address);
    bt_device_->AddUUID(ash::quick_pair::kFastPairBluetoothUuid);

    bt_device_->SetServiceDataForUUID(ash::quick_pair::kFastPairBluetoothUuid,
                                      {1, 2, 3});
    auto* bt_device_ptr = bt_device_.get();
    adapter_->AddMockDevice(std::move(bt_device_));
    adapter_->NotifyDevicePairedChanged(bt_device_ptr, new_paired_status);
  }

  void PairNonFastPairDeviceWithClassicBluetooth() {
    fake_data_parser_->SetHexModelIdFromServiceData(kValidModelId);
    bt_device_ = CreateTestBluetoothDevice(kTestDeviceAddress1);
    auto* bt_device_ptr = bt_device_.get();
    adapter_->AddMockDevice(std::move(bt_device_));
    adapter_->NotifyDevicePairedChanged(bt_device_ptr,
                                        /*new_paired_status*/ true);
  }

  void SetCheckAccountKeyResult() {
    nearby::fastpair::Device fp_device;
    fp_device.set_id(kModelIdLong);
    nearby::fastpair::GetObservedDeviceResponse response;
    response.mutable_device()->CopyFrom(fp_device);

    device_metadata_ =
        std::make_unique<DeviceMetadata>(std::move(response), gfx::Image());

    PairingMetadata pairing_metadata(device_metadata_.get(),
                                     std::vector<uint8_t>());
    repository_->SetCheckAccountKeysResult(pairing_metadata);
  }

  void OnRetroactivePairFound(scoped_refptr<Device> device) override {
    retroactive_pair_found_ = true;
  }

  bool retroactive_pair_found() { return retroactive_pair_found_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  bool retroactive_pair_found_ = false;

  scoped_refptr<RetroactivePairingDetectorFakeBluetoothAdapter> adapter_;
  std::unique_ptr<PairerBroker> pairer_broker_;
  MockPairerBroker* mock_pairer_broker_ = nullptr;

  FakeFastPairDataParser* fake_data_parser_ = nullptr;

  scoped_refptr<Device> device_;
  std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>> bt_device_;

  absl::optional<NotDiscoverableAdvertisement> advertisement_;
  std::unique_ptr<DeviceMetadata> device_metadata_;

  std::unique_ptr<FakeFastPairRepository> repository_;
  std::unique_ptr<device::BluetoothDevice> bluetooth_device_;
  std::unique_ptr<QuickPairProcessManager> process_manager_;

  mojo::SharedRemote<mojom::FastPairDataParser> data_parser_remote_;
  mojo::PendingRemote<mojom::FastPairDataParser> fast_pair_data_parser_;
  std::unique_ptr<FastPairDataParser> data_parser_;

  std::unique_ptr<RetroactivePairingDetector> retroactive_pairing_detector_;
};

TEST_F(RetroactivePairingDetectorTest,
       DevicePaired_WithFastPair_SameAddresses) {
  PairFastPairDeviceWithFastPair(kTestDeviceAddress1);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/kValidModelId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest,
       DevicePaired_WithFastPair_DifferentAddresses) {
  PairFastPairDeviceWithFastPair(kTestDeviceAddress2);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/kValidModelId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest, DeviceUnpaired) {
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/false, kTestDeviceAddress1,
      /*set_public_key=*/false, /*model_id=*/kValidModelId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest, NewDevicePaired_NoFPServiceData) {
  PairNonFastPairDeviceWithClassicBluetooth();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest, NewDevicePaired_WithModelID_NoMetadata) {
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/kValidModelId);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest,
       NewDevicePaired_WithModelID_AntiSpoofingKey) {
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/true,
      /*model_id=*/kValidModelId);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest,
       NewDevicePaired_NoModelID_NoAdvertisementData) {
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest,
       NewDevicePaired_NoModelID_NoAdvertisementDataUi) {
  SetParseAdvertisementResult(/*show_ui=*/false);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest,
       NewDevicePaired_NoModelID_AdvertisementDataUi_FailedCheckAccountKey) {
  SetParseAdvertisementResult(/*show_ui=*/true);
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(retroactive_pair_found());
}

TEST_F(RetroactivePairingDetectorTest,
       NewDevicePaired_NoModelID_AdvertisementDataUi_CheckAccountKeySuccess) {
  SetParseAdvertisementResult(/*show_ui=*/true);
  SetCheckAccountKeyResult();
  PairFastPairDeviceWithClassicBluetooth(
      /*new_paired_status=*/true, kTestDeviceAddress1, /*set_public_key=*/false,
      /*model_id=*/absl::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(retroactive_pair_found());
}

}  // namespace quick_pair
}  // namespace ash
