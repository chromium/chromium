// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository_impl.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/repository/fake_device_metadata_http_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/device_id_map.h"
#include "ash/quick_pair/repository/fast_pair/device_image_store.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fake_footprints_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/mock_fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair/proto_conversions.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/prefs/testing_pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr char kValidModelId[] = "abc";
constexpr char kInvalidModelId[] = "666";
constexpr char kTestModelId[] = "test_model_id";
constexpr char kTestDeviceId[] = "test_ble_device_id";
constexpr char kTestBLEAddress[] = "test_ble_address";
constexpr char kTestClassicAddress[] = "test_classic_address";
constexpr char kFirstSavedMacAddress[] = "00:11:22:33:44";
const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
const std::vector<uint8_t> kAccountKey2{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                        0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                        0x77, 0x77, 0x88, 0x88};
const std::vector<uint8_t> kFilterBytes1{0x0A, 0x42, 0x88, 0x10};
const uint8_t salt = 0xC7;

}  // namespace

namespace ash {
namespace quick_pair {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Return;

class FastPairRepositoryImplTest : public AshTestBase {
 public:
  FastPairRepositoryImplTest()
      : adapter_(new testing::NiceMock<device::MockBluetoothAdapter>),
        ble_bluetooth_device_(adapter_.get(),
                              0,
                              "Test ble name",
                              kTestBLEAddress,
                              false,
                              true),
        classic_bluetooth_device_(adapter_.get(),
                                  0,
                                  "Test classic name",
                                  kTestClassicAddress,
                                  false,
                                  true) {
    ON_CALL(ble_bluetooth_device_, GetIdentifier)
        .WillByDefault(Return(kTestDeviceId));
    ON_CALL(classic_bluetooth_device_, GetIdentifier)
        .WillByDefault(Return(kTestDeviceId));
    ON_CALL(*adapter_, GetDevice(kTestBLEAddress))
        .WillByDefault(Return(&ble_bluetooth_device_));
    ON_CALL(*adapter_, GetDevice(kTestClassicAddress))
        .WillByDefault(Return(&classic_bluetooth_device_));
  }

  void SetUp() override {
    AshTestBase::SetUp();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    device_ = base::MakeRefCounted<Device>(kTestModelId, kTestBLEAddress,
                                           Protocol::kFastPairInitial);
    device_->set_classic_address(kTestClassicAddress);

    auto http_fetcher = std::make_unique<FakeDeviceMetadataHttpFetcher>();
    metadata_http_fetcher_ = http_fetcher.get();

    auto device_metadata_fetcher =
        std::make_unique<DeviceMetadataFetcher>(std::move(http_fetcher));
    device_metadata_fetcher_ = device_metadata_fetcher.get();

    auto footprints_fetcher = std::make_unique<FakeFootprintsFetcher>();
    footprints_fetcher_ = footprints_fetcher.get();

    auto image_decoder = std::make_unique<MockFastPairImageDecoder>();
    image_decoder_ = image_decoder.get();
    test_image_ = gfx::test::CreateImage(100, 100);
    ON_CALL(*image_decoder_, DecodeImage(_, _, _))
        .WillByDefault(RunOnceCallback<2>(test_image_));

    auto device_id_map = std::make_unique<DeviceIdMap>();
    device_id_map_ = device_id_map.get();

    auto device_image_store =
        std::make_unique<DeviceImageStore>(image_decoder_);
    device_image_store_ = device_image_store.get();

    auto saved_device_registry = std::make_unique<SavedDeviceRegistry>();
    saved_device_registry_ = saved_device_registry.get();

    fast_pair_repository_ = std::make_unique<FastPairRepositoryImpl>(
        std::move(device_metadata_fetcher), std::move(footprints_fetcher),
        std::move(image_decoder), std::move(device_id_map),
        std::move(device_image_store), std::move(saved_device_registry));

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    ON_CALL(browser_delegate_, GetActivePrefService())
        .WillByDefault(testing::Return(pref_service_.get()));
    SavedDeviceRegistry::RegisterProfilePrefs(pref_service_->registry());
    DeviceIdMap::RegisterLocalStatePrefs(pref_service_->registry());
  }

  void VerifyMetadata(base::OnceClosure on_complete,
                      DeviceMetadata* device_metadata,
                      bool should_retry) {
    EXPECT_NE(nullptr, device_metadata);
    std::move(on_complete).Run();
  }

  void VerifyMetadataFailure(base::OnceClosure on_complete,
                             bool expected_retry,
                             DeviceMetadata* device_metadata,
                             bool should_retry) {
    EXPECT_EQ(nullptr, device_metadata);
    EXPECT_EQ(expected_retry, should_retry);
    std::move(on_complete).Run();
  }

  void VerifyAccountKeyCheck(base::OnceClosure on_complete,
                             bool expected_result,
                             absl::optional<PairingMetadata> pairing_metadata) {
    if (expected_result) {
      EXPECT_NE(absl::nullopt, pairing_metadata);
    } else {
      EXPECT_EQ(absl::nullopt, pairing_metadata);
    }
    std::move(on_complete).Run();
  }

  void GetSavedDevicesCallback(
      nearby::fastpair::OptInStatus status,
      std::vector<nearby::fastpair::FastPairDevice> devices) {
    status_ = status;
    devices_ = devices;
  }

 protected:
  std::unique_ptr<FastPairRepositoryImpl> fast_pair_repository_;
  nearby::fastpair::OptInStatus status_;
  std::vector<nearby::fastpair::FastPairDevice> devices_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  testing::NiceMock<device::MockBluetoothDevice> ble_bluetooth_device_;
  testing::NiceMock<device::MockBluetoothDevice> classic_bluetooth_device_;
  scoped_refptr<Device> device_;
  gfx::Image test_image_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  MockQuickPairBrowserDelegate browser_delegate_;

  DeviceMetadataFetcher* device_metadata_fetcher_;
  FakeDeviceMetadataHttpFetcher* metadata_http_fetcher_;
  FakeFootprintsFetcher* footprints_fetcher_;
  MockFastPairImageDecoder* image_decoder_;
  DeviceIdMap* device_id_map_;
  DeviceImageStore* device_image_store_;
  SavedDeviceRegistry* saved_device_registry_;

  base::WeakPtrFactory<FastPairRepositoryImplTest> weak_ptr_factory_{this};
};

TEST_F(FastPairRepositoryImplTest, GetDeviceMetadata) {
  auto run_loop = std::make_unique<base::RunLoop>();
  fast_pair_repository_->GetDeviceMetadata(
      kValidModelId,
      base::BindOnce(&FastPairRepositoryImplTest::VerifyMetadata,
                     base::Unretained(this), run_loop->QuitClosure()));
  run_loop->Run();
  EXPECT_EQ(1, metadata_http_fetcher_->num_gets());

  run_loop = std::make_unique<base::RunLoop>();
  fast_pair_repository_->GetDeviceMetadata(
      kValidModelId,
      base::BindOnce(&FastPairRepositoryImplTest::VerifyMetadata,
                     base::Unretained(this), run_loop->QuitClosure()));
  run_loop->Run();
  // Indicates that the cache was used instead of a second GET.
  EXPECT_EQ(1, metadata_http_fetcher_->num_gets());
}

TEST_F(FastPairRepositoryImplTest, GetDeviceMetadata_Failed_Retryable) {
  base::RunLoop run_loop;
  metadata_http_fetcher_->set_network_error(true);
  fast_pair_repository_->GetDeviceMetadata(
      kInvalidModelId,
      base::BindOnce(&FastPairRepositoryImplTest::VerifyMetadataFailure,
                     base::Unretained(this), run_loop.QuitClosure(),
                     /*expected_retry=*/true));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest, GetDeviceMetadata_Failed_NotRetryable) {
  base::RunLoop run_loop;
  fast_pair_repository_->GetDeviceMetadata(
      kInvalidModelId,
      base::BindOnce(&FastPairRepositoryImplTest::VerifyMetadataFailure,
                     base::Unretained(this), run_loop.QuitClosure(),
                     /*expected_retry=*/false));
  run_loop.Run();
  EXPECT_EQ(1, metadata_http_fetcher_->num_gets());
}

TEST_F(FastPairRepositoryImplTest, CheckAccountKeys_NoMatch) {
  AccountKeyFilter filter(kFilterBytes1, {salt});

  auto run_loop = std::make_unique<base::RunLoop>();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop->QuitClosure(),
                             /*expected_result=*/false));
  run_loop->Run();
}

TEST_F(FastPairRepositoryImplTest, CheckAccountKeys_Match) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse device;
  DeviceMetadata metadata(device, gfx::Image());

  // FakeFootprintsFetcher APIs are actually synchronous.
  footprints_fetcher_->AddUserFastPairInfo(
      BuildFastPairInfo(kValidModelId, kAccountKey1, &metadata),
      base::DoNothing());

  auto run_loop = std::make_unique<base::RunLoop>();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop->QuitClosure(),
                             /*expected_result=*/true));
  run_loop->Run();
}

TEST_F(FastPairRepositoryImplTest, AssociateAccountKey_InvalidId) {
  auto device = base::MakeRefCounted<Device>(kInvalidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->AssociateAccountKey(device, kAccountKey1);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest, AssociateAccountKey_ValidId) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->AssociateAccountKey(device, kAccountKey1);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest,
       AssociateAccountKeyLocally_InvalidNoAccountKey) {
  auto device = base::MakeRefCounted<Device>(kInvalidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  ASSERT_FALSE(fast_pair_repository_->AssociateAccountKeyLocally(device));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest, AssociateAccountKeyLocally_ValidAccountKey) {
  auto device = base::MakeRefCounted<Device>(kInvalidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  device->SetAdditionalData(Device::AdditionalDataType::kAccountKey,
                            kAccountKey1);
  ASSERT_TRUE(fast_pair_repository_->AssociateAccountKeyLocally(device));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest, DeleteAssociatedDevice_Valid) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->AssociateAccountKey(device, kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true))).Times(1);
  fast_pair_repository_->DeleteAssociatedDevice(
      classic_bluetooth_device_.GetAddress(), callback.Get());

  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest, DeleteAssociatedDevice_Invalid) {
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->DeleteAssociatedDevice(
      classic_bluetooth_device_.GetAddress(), callback.Get());
}

TEST_F(FastPairRepositoryImplTest, DeleteAssociatedDeviceByAccountKey_Valid) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->AssociateAccountKey(device, kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true))).Times(1);
  fast_pair_repository_->DeleteAssociatedDeviceByAccountKey(kAccountKey1,
                                                            callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest, FetchDeviceImages) {
  ASSERT_FALSE(device_id_map_->GetModelIdForDeviceId(kTestDeviceId));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->FetchDeviceImages(device);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(device_id_map_->GetModelIdForDeviceId(kTestDeviceId));
  ASSERT_TRUE(fast_pair_repository_->GetImagesForDevice(kTestDeviceId));
}

TEST_F(FastPairRepositoryImplTest, PersistDeviceImages) {
  ASSERT_FALSE(device_id_map_->GetModelIdForDeviceId(kTestDeviceId));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->FetchDeviceImages(device);
  fast_pair_repository_->PersistDeviceImages(device);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(device_id_map_->GetModelIdForDeviceId(kTestDeviceId));
  ASSERT_TRUE(fast_pair_repository_->GetImagesForDevice(kTestDeviceId));
}

TEST_F(FastPairRepositoryImplTest, EvictDeviceImages) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->FetchDeviceImages(device);
  fast_pair_repository_->PersistDeviceImages(device);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(device_id_map_->GetModelIdForDeviceId(kTestDeviceId));
  ASSERT_TRUE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  fast_pair_repository_->EvictDeviceImages(&classic_bluetooth_device_);
  base::RunLoop().RunUntilIdle();

  device_id_map_->RefreshCacheForTest();
  ASSERT_FALSE(device_id_map_->GetModelIdForDeviceId(kTestDeviceId));
}

TEST_F(FastPairRepositoryImplTest, UpdateOptInStatus_OptedIn) {
  base::MockCallback<base::OnceCallback<void(bool)>> callback1;
  EXPECT_CALL(callback1, Run(true)).Times(1);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN, callback1.Get());
  base::RunLoop().RunUntilIdle();

  base::MockCallback<base::OnceCallback<void(nearby::fastpair::OptInStatus)>>
      callback2;
  EXPECT_CALL(callback2,
              Run(testing::Eq(nearby::fastpair::OptInStatus::STATUS_OPTED_IN)))
      .Times(1);
  fast_pair_repository_->CheckOptInStatus(callback2.Get());
}

TEST_F(FastPairRepositoryImplTest, UpdateOptInStatus_OptedOut) {
  base::MockCallback<base::OnceCallback<void(bool)>> callback1;
  EXPECT_CALL(callback1, Run(true)).Times(1);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT, callback1.Get());
  base::RunLoop().RunUntilIdle();

  base::MockCallback<base::OnceCallback<void(nearby::fastpair::OptInStatus)>>
      callback2;
  EXPECT_CALL(callback2,
              Run(testing::Eq(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT)))
      .Times(1);
  fast_pair_repository_->CheckOptInStatus(callback2.Get());
}

TEST_F(FastPairRepositoryImplTest, UpdateOptInStatus_StatusUnknown) {
  base::MockCallback<base::OnceCallback<void(bool)>> callback1;
  EXPECT_CALL(callback1, Run(true)).Times(1);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN, callback1.Get());
  base::RunLoop().RunUntilIdle();

  base::MockCallback<base::OnceCallback<void(nearby::fastpair::OptInStatus)>>
      callback2;
  EXPECT_CALL(callback2,
              Run(testing::Eq(nearby::fastpair::OptInStatus::STATUS_UNKNOWN)))
      .Times(1);
  fast_pair_repository_->CheckOptInStatus(callback2.Get());
}

TEST_F(FastPairRepositoryImplTest, UpdateOptInStatus_NoFootprintsResponse) {
  footprints_fetcher_->SetGetUserDevicesResponse(absl::nullopt);
  base::MockCallback<base::OnceCallback<void(nearby::fastpair::OptInStatus)>>
      callback;
  EXPECT_CALL(callback,
              Run(testing::Eq(nearby::fastpair::OptInStatus::STATUS_UNKNOWN)))
      .Times(1);
  fast_pair_repository_->CheckOptInStatus(callback.Get());
}

TEST_F(FastPairRepositoryImplTest, UpdateOptInStatus_OptedInUpdateFailed) {
  footprints_fetcher_->SetAddUserFastPairInfoResult(/*add_user_result=*/false);
  base::MockCallback<base::OnceCallback<void(bool)>> callback1;
  EXPECT_CALL(callback1, Run(false)).Times(1);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN, callback1.Get());
  base::RunLoop().RunUntilIdle();

  base::MockCallback<base::OnceCallback<void(nearby::fastpair::OptInStatus)>>
      callback2;
  EXPECT_CALL(callback2,
              Run(testing::Eq(nearby::fastpair::OptInStatus::STATUS_UNKNOWN)))
      .Times(1);
  fast_pair_repository_->CheckOptInStatus(callback2.Get());
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_OptedIn) {
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress);
  fast_pair_repository_->AssociateAccountKey(device, kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  base::RunLoop().RunUntilIdle();

  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN, status_);
  EXPECT_EQ(1u, devices_.size());
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_OptedOut) {
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT, status_);
  EXPECT_EQ(0u, devices_.size());
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_OptStatusUnknown) {
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_UNKNOWN, status_);
  EXPECT_EQ(0u, devices_.size());
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_MissingResponse) {
  footprints_fetcher_->SetGetUserDevicesResponse(absl::nullopt);
  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_UNKNOWN, status_);
  EXPECT_EQ(0u, devices_.size());
}

TEST_F(FastPairRepositoryImplTest, IsAccountKeyPairedLocally) {
  saved_device_registry_->SaveAccountKey(kFirstSavedMacAddress, kAccountKey1);
  EXPECT_TRUE(fast_pair_repository_->IsAccountKeyPairedLocally(kAccountKey1));
  EXPECT_FALSE(fast_pair_repository_->IsAccountKeyPairedLocally(kAccountKey2));
}

}  // namespace quick_pair
}  // namespace ash
