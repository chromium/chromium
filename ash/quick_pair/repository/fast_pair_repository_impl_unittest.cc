// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair_repository_impl.h"

#include <optional>

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"
#include "ash/quick_pair/common/protocol.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/repository/fake_device_metadata_http_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/device_address_map.h"
#include "ash/quick_pair/repository/fast_pair/device_image_store.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/fake_footprints_fetcher.h"
#include "ash/quick_pair/repository/fast_pair/mock_fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair/pending_write_store.h"
#include "ash/quick_pair/repository/fast_pair/proto_conversions.h"
#include "ash/quick_pair/repository/fast_pair/saved_device_registry.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/quick_pair/public/cpp/account_key_filter.h"
#include "components/prefs/testing_pref_service.h"
#include "crypto/sha2.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr int kBluetoothAddressSize = 6;
constexpr char kValidModelId[] = "abc";
constexpr char kInvalidModelId[] = "666";
constexpr char kDeviceDisplayName[] = "test_nickname";
constexpr char kDeviceDisplayName2[] = "second_test_nickname";
constexpr char kTestModelId[] = "test_model_id";
constexpr char kTestDeviceId[] = "test_ble_device_id";
constexpr char kTestBLEAddress[] = "00:11:22:33:45:11";
constexpr char kTestBLEAddress2[] = "00:11:22:33:45:77";
constexpr char kTestClassicAddress1[] = "00:11:22:33:44:55";
constexpr char kTestClassicAddress2[] = "00:11:22:33:44:66";
constexpr char kTestClassicAddress3[] = "04:CB:88:1E:56:19";
constexpr char kBase64ExpectedSha256Hash[] =
    "gVzzRtZjwYv8lO8xwWnWW2uw/stV6RdEUhv3cIN3nH4=";
constexpr char kBase64ForgetPatternSha256Hash[] =
    "8PDw8NZjwYv8lO8xwWnWW2uw/stV6RdEUhv3cIN3nH4=";
constexpr char kBase64AccountKey[] = "BAcDiEH56/Mq3hW7OKUctA==";
const std::vector<uint8_t> kAccountKey1{0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                        0x77, 0x88, 0x99, 0x00, 0xAA, 0xBB,
                                        0xCC, 0xDD, 0xEE, 0xFF};
const std::vector<uint8_t> kAccountKey2{0x11, 0x11, 0x22, 0x22, 0x33, 0x33,
                                        0x44, 0x44, 0x55, 0x55, 0x66, 0x66,
                                        0x77, 0x77, 0x88, 0x88};
const std::vector<uint8_t> kFilterBytes1{0x0A, 0x42, 0x88, 0x10};
const uint8_t salt = 0xC7;

const char kSavedDeviceGetDevicesResultMetricName[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.GetSavedDevices.Result";
constexpr char kRetroactiveSuccessFunnelMetric[] =
    "FastPair.RetroactivePairing";

// Computes and returns the Sha256 of the concatenation of the given
// |account_key| and |mac_address|.
std::string GenerateSha256AccountKeyMacAddress(const std::string& account_key,
                                               const std::string& mac_address) {
  std::vector<uint8_t> concat_bytes(account_key.begin(), account_key.end());
  std::vector<uint8_t> mac_address_bytes;
  mac_address_bytes.resize(kBluetoothAddressSize);
  device::ParseBluetoothAddress(mac_address, mac_address_bytes);

  concat_bytes.insert(concat_bytes.end(), mac_address_bytes.begin(),
                      mac_address_bytes.end());
  std::array<uint8_t, crypto::kSHA256Length> hashed =
      crypto::SHA256Hash(concat_bytes);

  return std::string(hashed.begin(), hashed.end());
}

std::string Base64Decode(const std::string& encoded) {
  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  return decoded;
}

}  // namespace

namespace ash {
namespace quick_pair {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Return;

class FastPairRepositoryImplTest : public AshTestBase {
 public:
  FastPairRepositoryImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        adapter_(new testing::NiceMock<device::MockBluetoothAdapter>),
        ble_bluetooth_device_(adapter_.get(),
                              0,
                              "Test ble name",
                              kTestBLEAddress,
                              false,
                              true),
        classic_bluetooth_device_(adapter_.get(),
                                  0,
                                  "Test classic name",
                                  kTestClassicAddress1,
                                  false,
                                  true) {
    ON_CALL(ble_bluetooth_device_, GetIdentifier)
        .WillByDefault(Return(kTestDeviceId));
    ON_CALL(classic_bluetooth_device_, GetIdentifier)
        .WillByDefault(Return(kTestDeviceId));
    ON_CALL(ble_bluetooth_device_, IsPaired)
        .WillByDefault(testing::Return(true));
    ON_CALL(classic_bluetooth_device_, IsPaired)
        .WillByDefault(testing::Return(true));
    ON_CALL(*adapter_, GetDevices).WillByDefault(testing::Return(device_list_));
    ON_CALL(*adapter_, GetDevice(kTestBLEAddress))
        .WillByDefault(Return(&ble_bluetooth_device_));
    ON_CALL(*adapter_, GetDevice(kTestClassicAddress1))
        .WillByDefault(Return(&classic_bluetooth_device_));
  }

  void SetUp() override {
    AshTestBase::SetUp();
    NetworkHandler::Initialize();
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);
    device_ = base::MakeRefCounted<Device>(kTestModelId, kTestBLEAddress,
                                           Protocol::kFastPairInitial);
    device_->set_classic_address(kTestClassicAddress1);

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
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>(test_image_));

    auto device_address_map = std::make_unique<DeviceAddressMap>();
    device_address_map_ = device_address_map.get();

    auto device_image_store =
        std::make_unique<DeviceImageStore>(image_decoder_);
    device_image_store_ = device_image_store.get();

    auto pending_write_store = std::make_unique<PendingWriteStore>();
    pending_write_store_ = pending_write_store.get();

    auto saved_device_registry =
        std::make_unique<SavedDeviceRegistry>(adapter_.get());
    saved_device_registry_ = saved_device_registry.get();

    fast_pair_repository_ = std::make_unique<FastPairRepositoryImpl>(
        adapter_, std::move(device_metadata_fetcher),
        std::move(footprints_fetcher), std::move(image_decoder),
        std::move(device_address_map), std::move(device_image_store),
        std::move(saved_device_registry), std::move(pending_write_store));
    FastPairRepository::SetInstanceForTesting(fast_pair_repository_.get());
  }

  void TearDown() override {
    fast_pair_repository_.reset();
    NetworkHandler::Shutdown();
    AshTestBase::TearDown();
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
                             std::optional<PairingMetadata> pairing_metadata) {
    if (expected_result) {
      EXPECT_NE(std::nullopt, pairing_metadata);
    } else {
      EXPECT_EQ(std::nullopt, pairing_metadata);
    }
    std::move(on_complete).Run();
  }

  void GetSavedDevicesCallback(
      nearby::fastpair::OptInStatus status,
      std::vector<nearby::fastpair::FastPairDevice> devices) {
    status_ = status;
    devices_ = devices;
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  NetworkStateTestHelper helper_{/*use_default_devices_and_services=*/true};
  std::unique_ptr<FastPairRepositoryImpl> fast_pair_repository_;
  nearby::fastpair::OptInStatus status_;
  std::vector<nearby::fastpair::FastPairDevice> devices_;
  base::HistogramTester histogram_tester_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> adapter_;
  testing::NiceMock<device::MockBluetoothDevice> ble_bluetooth_device_;
  testing::NiceMock<device::MockBluetoothDevice> classic_bluetooth_device_;
  device::BluetoothAdapter::ConstDeviceList device_list_{
      &ble_bluetooth_device_, &classic_bluetooth_device_};
  scoped_refptr<Device> device_;
  gfx::Image test_image_;

  raw_ptr<DeviceMetadataFetcher, DanglingUntriaged> device_metadata_fetcher_;
  raw_ptr<FakeDeviceMetadataHttpFetcher, DanglingUntriaged>
      metadata_http_fetcher_;
  raw_ptr<FakeFootprintsFetcher, DanglingUntriaged> footprints_fetcher_;
  raw_ptr<MockFastPairImageDecoder, DanglingUntriaged> image_decoder_;
  raw_ptr<DeviceAddressMap, DanglingUntriaged> device_address_map_;
  raw_ptr<DeviceImageStore, DanglingUntriaged> device_image_store_;
  raw_ptr<PendingWriteStore, DanglingUntriaged> pending_write_store_;
  raw_ptr<SavedDeviceRegistry, DanglingUntriaged> saved_device_registry_;

  base::WeakPtrFactory<FastPairRepositoryImplTest> weak_ptr_factory_{this};
};

TEST_F(FastPairRepositoryImplTest, GetDeviceMetadata) {
  {
    auto run_loop = base::RunLoop();
    fast_pair_repository_->GetDeviceMetadata(
        kValidModelId,
        base::BindOnce(&FastPairRepositoryImplTest::VerifyMetadata,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(1, metadata_http_fetcher_->num_gets());

  {
    auto run_loop = base::RunLoop();
    fast_pair_repository_->GetDeviceMetadata(
        kValidModelId,
        base::BindOnce(&FastPairRepositoryImplTest::VerifyMetadata,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

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

  auto run_loop = base::RunLoop();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/false));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest, CheckAccountKeys_Match) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse device;
  DeviceMetadata metadata(device, gfx::Image());

  // FakeFootprintsFetcher APIs are actually synchronous.
  footprints_fetcher_->AddUserFastPairInfo(
      BuildFastPairInfo(kValidModelId, kAccountKey1, kTestClassicAddress1,
                        kDeviceDisplayName, &metadata),
      base::DoNothing());

  auto run_loop = base::RunLoop();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest, CheckAccountKeys_Match_No_Name) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse device;
  DeviceMetadata metadata(device, gfx::Image());

  // FakeFootprintsFetcher APIs are actually synchronous.
  // The |device_| display name is not set so this will mimic when we fail to
  // get a |display_name| for the device and have to fall back on using the
  // metadata name when creating the proto.
  footprints_fetcher_->AddUserFastPairInfo(
      BuildFastPairInfo(kValidModelId, kAccountKey1, kTestClassicAddress1,
                        device_->display_name(), &metadata),
      base::DoNothing());

  auto run_loop = base::RunLoop();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest, CheckAccountKeys_SkipForgetPattern) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse details;
  DeviceMetadata metadata(details, gfx::Image());

  // In this test, we use a known "marked as forgotten" hash
  // from Android. Note that Android also leaves the account key
  // blank; however, we add a matching account key here to test the logic
  // of detecting the Forget pattern in the hash, which should be cause
  // the device to be skipped in CheckAccountKeys.
  nearby::fastpair::FastPairInfo info =
      BuildFastPairInfo(kValidModelId, kAccountKey1, kTestClassicAddress1,
                        kDeviceDisplayName, &metadata);
  auto* device = info.mutable_device();
  device->set_sha256_account_key_public_address(
      Base64Decode(kBase64ForgetPatternSha256Hash));
  footprints_fetcher_->AddUserFastPairInfo(info, base::DoNothing());

  auto run_loop = std::make_unique<base::RunLoop>();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop->QuitClosure(),
                             /*expected_result=*/false));
  run_loop->Run();
}

TEST_F(FastPairRepositoryImplTest, UpdateStaleUserDeviceCache) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  auto run_loop = base::RunLoop();

  // Check for the device, this will also load the device into the cache
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  base::RunLoop().RunUntilIdle();

  // Remove the device directly from footprints. This is equivalent to the
  // device being removed on an Android phone or another Chromebook
  footprints_fetcher_->DeleteUserDevice(base::HexEncode(kAccountKey1),
                                        base::DoNothing());

  // 29 minutes later, device is still in the cache
  task_environment()->FastForwardBy(base::Minutes(29));
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  base::RunLoop().RunUntilIdle();

  // After >30 minutes, cache will have gone stale so device will be removed
  task_environment()->FastForwardBy(base::Seconds(61));
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/false));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest, UseStaleCache) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  auto run_loop = base::RunLoop();

  // Check for the device, this will also load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));

  // Remove the device directly from footprints. This is equivalent to the
  // device being removed on an Android phone or another Chromebook.
  footprints_fetcher_->DeleteUserDevice(base::HexEncode(kAccountKey1),
                                        base::DoNothing());

  // Set the response to replicate an error getting devices from the server.
  footprints_fetcher_->SetGetUserDevicesResponse(std::nullopt);

  // After >30 minutes, cache is stale but we will fail to get devices from
  // the server so we use the stale cache with the device still present.
  task_environment()->FastForwardBy(base::Minutes(31));
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest, GetDeviceNameFromCache) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairSubsequent);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  auto run_loop = base::RunLoop();

  // Check for the device, this will load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  run_loop.Run();

  device->set_display_name(fast_pair_repository_->GetDeviceDisplayNameFromCache(
      device->account_key().value()));
  ASSERT_EQ(kDeviceDisplayName, device->display_name());
}

TEST_F(FastPairRepositoryImplTest, ChangeDeviceNameFromCache) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  auto run_loop = base::RunLoop();

  // Check for the device, this will load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));

  device->set_display_name(fast_pair_repository_->GetDeviceDisplayNameFromCache(
      device->account_key().value()));
  ASSERT_EQ(device->display_name(), kDeviceDisplayName);

  fast_pair_repository_->UpdateAssociatedDeviceFootprintsName(
      kTestClassicAddress1, kDeviceDisplayName2, /*cache_may_be_stale=*/true);

  // Check for the device, this will load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));

  device->set_display_name(fast_pair_repository_->GetDeviceDisplayNameFromCache(
      device->account_key().value()));
  ASSERT_EQ(device->display_name(), kDeviceDisplayName2);
}

TEST_F(FastPairRepositoryImplTest, ChangeDeviceNameNotInCache) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  auto run_loop = base::RunLoop();

  fast_pair_repository_->UpdateAssociatedDeviceFootprintsName(
      kTestClassicAddress1, kDeviceDisplayName2, /*cache_may_be_stale=*/true);
  base::RunLoop().RunUntilIdle();

  // Check for the device, this will load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  base::RunLoop().RunUntilIdle();
  device->set_display_name(fast_pair_repository_->GetDeviceDisplayNameFromCache(
      device->account_key().value()));
  ASSERT_EQ(device->display_name(), kDeviceDisplayName2);
}

TEST_F(FastPairRepositoryImplTest,
       UpdateAssociatedDeviceFootprintsName_No_Matching_Account_key) {
  AccountKeyFilter filter(kFilterBytes1, {salt});

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  fast_pair_repository_->UpdateAssociatedDeviceFootprintsName(
      kTestClassicAddress1, kDeviceDisplayName2, /*cache_may_be_stale=*/true);
  base::RunLoop().RunUntilIdle();

  auto run_loop = base::RunLoop();

  // Check for the device, this will load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/false));

  device->set_display_name(fast_pair_repository_->GetDeviceDisplayNameFromCache(
      device->account_key().value()));
}

TEST_F(FastPairRepositoryImplTest, LocalRemoveDeviceUpdatesCache) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  auto run_loop = base::RunLoop();

  // Check for the device, this will also load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));

  // Remove the device as if this chromebook was removing it. This should
  // invalidate the cache so the device will be removed there as well.
  fast_pair_repository_->DeleteAssociatedDevice(
      classic_bluetooth_device_.GetAddress(), base::DoNothing());

  // Device should not appear in the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/false));
  run_loop.Run();
}

TEST_F(FastPairRepositoryImplTest,
       WriteAccountAssociationToFootprints_InvalidId) {
  auto device = base::MakeRefCounted<Device>(kInvalidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest,
       WriteAccountAssociationToFootprints_ValidId) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kSaveComplete),
            0);
}

TEST_F(FastPairRepositoryImplTest,
       WriteAccountAssociationToFootprints_LogRetroactiveSuccessFunnel) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairRetroactive);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kSaveComplete),
            1);
}

TEST_F(FastPairRepositoryImplTest, AssociateAccountKeyAndCheckName) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  auto run_loop = base::RunLoop();

  // Check for the device, this will load the device into the cache.
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/true));
  run_loop.Run();

  // Device Account Key is set for testing purposes only.
  device->set_account_key(kAccountKey1);
  device->set_display_name(fast_pair_repository_->GetDeviceDisplayNameFromCache(
      device->account_key().value()));
  ASSERT_EQ(device->display_name(), kDeviceDisplayName);
}

TEST_F(FastPairRepositoryImplTest,
       WriteAccountAssociationToLocalRegistry_InvalidNoAccountKey) {
  auto device = base::MakeRefCounted<Device>(kInvalidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  ASSERT_FALSE(
      fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device));
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest,
       WriteAccountAssociationToLocalRegistry_ValidAccountKey) {
  auto device = base::MakeRefCounted<Device>(kInvalidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  ASSERT_TRUE(
      fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device));
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
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
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
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());
}

TEST_F(FastPairRepositoryImplTest, DeleteAssociatedDevice_Invalid) {
  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->DeleteAssociatedDevice(
      classic_bluetooth_device_.GetAddress(), callback.Get());

  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());
}

TEST_F(FastPairRepositoryImplTest, DeleteAssociatedDeviceByAccountKey_Valid) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true))).Times(1);
  fast_pair_repository_->DeleteAssociatedDeviceByAccountKey(kAccountKey1,
                                                            callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());
}

TEST_F(FastPairRepositoryImplTest, RetriesForgetDevice_AfterNetworkAvailable) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);

  // Mock an error due to Network failure.
  footprints_fetcher_->SetDeleteUserDeviceResult(false);
  fast_pair_repository_->DeleteAssociatedDevice(
      classic_bluetooth_device_.GetAddress(), callback.Get());

  base::RunLoop().RunUntilIdle();

  // The failed delete should be saved as a pending delete.
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(1u, pending_write_store_->GetPendingDeletes().size());

  // Reconnect to the Network, but fail again.
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // The delete, after another failed retry, should still be pending.
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(1u, pending_write_store_->GetPendingDeletes().size());

  // Reconnect to the Network, but within the 1 minute timeout.
  footprints_fetcher_->SetDeleteUserDeviceResult(true);
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // Since we don't retry within 1 minute, the delete should still be pending.
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(1u, pending_write_store_->GetPendingDeletes().size());

  // Mock waiting out the 1 minute timeout.
  task_environment()->FastForwardBy(base::Minutes(1));
  base::RunLoop().RunUntilIdle();

  // Reconnect to the Network, but after the 1 minute timeout.
  footprints_fetcher_->SetDeleteUserDeviceResult(true);
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // The delete, after a successful retry, should no longer be pending.
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_RetriesForgetDevice_AlreadyDeleted \
  DISABLED_RetriesForgetDevice_AlreadyDeleted
#else
#define MAYBE_RetriesForgetDevice_AlreadyDeleted \
  RetriesForgetDevice_AlreadyDeleted
#endif
TEST_F(FastPairRepositoryImplTest, MAYBE_RetriesForgetDevice_AlreadyDeleted) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_account_key(kAccountKey1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);

  // Mock an error due to Network failure.
  footprints_fetcher_->SetDeleteUserDeviceResult(false);
  fast_pair_repository_->DeleteAssociatedDevice(
      classic_bluetooth_device_.GetAddress(), callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(1u, pending_write_store_->GetPendingDeletes().size());

  // Mock Footprints getting updated by another CB/Android such that the
  // saved device is successfully deleted.
  footprints_fetcher_->SetDeleteUserDeviceResult(true);
  footprints_fetcher_->DeleteUserDevice(
      base::HexEncode(
          std::vector<uint8_t>(kAccountKey1.begin(), kAccountKey1.end())),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(1u, pending_write_store_->GetPendingDeletes().size());

  // Reconnect to the Network.
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // The delete, after a successful retry, should no longer be pending.
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());
}

TEST_F(FastPairRepositoryImplTest, RetriesForgetDevice_MultipleDevices) {
  auto device1 = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                              Protocol::kFastPairInitial);
  device1->set_classic_address(kTestClassicAddress1);
  device1->set_account_key(kAccountKey1);
  device1->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device1,
                                                             kAccountKey1);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device1);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  auto device2 = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress2,
                                              Protocol::kFastPairInitial);
  device2->set_classic_address(kTestClassicAddress2);
  device2->set_account_key(kAccountKey2);
  device2->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device2,
                                                             kAccountKey2);
  fast_pair_repository_->WriteAccountAssociationToLocalRegistry(device2);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey2));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());

  base::MockCallback<base::OnceCallback<void(bool)>> callback1;
  EXPECT_CALL(callback1, Run(testing::Eq(false))).Times(1);

  // Mock an error due to Network failure for device1.
  footprints_fetcher_->SetDeleteUserDeviceResult(false);
  fast_pair_repository_->DeleteAssociatedDevice(kTestClassicAddress1,
                                                callback1.Get());
  base::RunLoop().RunUntilIdle();

  // The failed delete should be saved as a pending delete.
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_EQ(1u, pending_write_store_->GetPendingDeletes().size());

  base::MockCallback<base::OnceCallback<void(bool)>> callback2;
  EXPECT_CALL(callback2, Run(testing::Eq(false))).Times(1);

  // Mock an error due to Network failure for device2.
  footprints_fetcher_->SetDeleteUserDeviceResult(false);
  fast_pair_repository_->DeleteAssociatedDevice(kTestClassicAddress2,
                                                callback2.Get());
  base::RunLoop().RunUntilIdle();

  // The failed deletes should be saved as pending deletes.
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey2));
  ASSERT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
  ASSERT_EQ(2u, pending_write_store_->GetPendingDeletes().size());

  // Reconnect to the Network.
  footprints_fetcher_->SetDeleteUserDeviceResult(true);
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // Both deletes should be retried and removed from pending write store.
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey2));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey2));
  ASSERT_EQ(0u, pending_write_store_->GetPendingDeletes().size());
}

TEST_F(FastPairRepositoryImplTest, FetchDeviceImages) {
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  fast_pair_repository_->FetchDeviceImages(device);

  ASSERT_TRUE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_TRUE(fast_pair_repository_->GetImagesForDevice(kTestClassicAddress1));
}

TEST_F(FastPairRepositoryImplTest, FetchDeviceImagesNoMacAddress) {
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  // Don't set the classic address of the device, which should result in the
  // mac address to model ID record failin to save.
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  fast_pair_repository_->FetchDeviceImages(device);

  // We expect that FetchDeviceImages will first be called while the classic
  // address of the device isn't set. Images should still be downloaded and
  // linked to the corresponding model ID.
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(fast_pair_repository_->GetImagesForDevice(kTestClassicAddress1));
  ASSERT_TRUE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  // Mimic the device now completing pairing, which should set the classic
  // address and attempt to FetchDeviceImages a second time.
  device->set_classic_address(kTestClassicAddress1);
  fast_pair_repository_->FetchDeviceImages(device);

  ASSERT_TRUE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_TRUE(fast_pair_repository_->GetImagesForDevice(kTestClassicAddress1));
}

TEST_F(FastPairRepositoryImplTest, PersistDeviceImages) {
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  fast_pair_repository_->FetchDeviceImages(device);
  ASSERT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kValidModelId));

  // Persisting should succeed, which allows the images to be found even after
  // the local caches are cleared (i.e. on logout).
  ASSERT_TRUE(fast_pair_repository_->PersistDeviceImages(device));

  device_address_map_->RefreshCacheForTest();
  device_image_store_->RefreshCacheForTest();
  ASSERT_TRUE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_TRUE(fast_pair_repository_->GetImagesForDevice(kTestClassicAddress1));
  ASSERT_TRUE(
      device_address_map_->HasPersistedRecordsForModelId(kValidModelId));
}

TEST_F(FastPairRepositoryImplTest, PersistDeviceImagesNoMacAddress) {
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  // Don't set the classic address of the device, which should result in the
  // mac address to model ID record failin to save.
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  fast_pair_repository_->FetchDeviceImages(device);

  // We expect that FetchDeviceImages will first be called while the classic
  // address of the device isn't set. Images should still be downloaded and
  // linked to the corresponding model ID.
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(fast_pair_repository_->GetImagesForDevice(kTestClassicAddress1));
  ASSERT_TRUE(device_image_store_->GetImagesForDeviceModel(kValidModelId));

  // Even though there are images saved that we could persist, we should not
  // persist them since we lack the corresponding mac address to model ID
  // mapping.
  ASSERT_FALSE(fast_pair_repository_->PersistDeviceImages(device));

  device_address_map_->RefreshCacheForTest();
  device_image_store_->RefreshCacheForTest();
  ASSERT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kValidModelId));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));
}

TEST_F(FastPairRepositoryImplTest, EvictDeviceImages) {
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  fast_pair_repository_->FetchDeviceImages(device);
  fast_pair_repository_->PersistDeviceImages(device);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_TRUE(device_image_store_->GetImagesForDeviceModel(kValidModelId));
  ASSERT_TRUE(
      device_address_map_->HasPersistedRecordsForModelId(kValidModelId));

  fast_pair_repository_->EvictDeviceImages(kTestClassicAddress1);
  ASSERT_FALSE(
      device_address_map_->HasPersistedRecordsForModelId(kValidModelId));

  device_address_map_->RefreshCacheForTest();
  device_image_store_->RefreshCacheForTest();
  ASSERT_FALSE(
      device_address_map_->GetModelIdForMacAddress(kTestClassicAddress1));
  ASSERT_FALSE(device_image_store_->GetImagesForDeviceModel(kValidModelId));
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_UpdateOptInStatus_OptedIn DISABLED_UpdateOptInStatus_OptedIn
#else
#define MAYBE_UpdateOptInStatus_OptedIn UpdateOptInStatus_OptedIn
#endif
TEST_F(FastPairRepositoryImplTest, MAYBE_UpdateOptInStatus_OptedIn) {
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

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_UpdateOptInStatus_StatusUnknown \
  DISABLED_UpdateOptInStatus_StatusUnknown
#else
#define MAYBE_UpdateOptInStatus_StatusUnknown UpdateOptInStatus_StatusUnknown
#endif
TEST_F(FastPairRepositoryImplTest, MAYBE_UpdateOptInStatus_StatusUnknown) {
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
  footprints_fetcher_->SetGetUserDevicesResponse(std::nullopt);
  base::MockCallback<base::OnceCallback<void(nearby::fastpair::OptInStatus)>>
      callback;
  EXPECT_CALL(callback,
              Run(testing::Eq(nearby::fastpair::OptInStatus::STATUS_UNKNOWN)))
      .Times(1);
  fast_pair_repository_->CheckOptInStatus(callback.Get());
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_UpdateOptInStatus_OptedInUpdateFailed \
  DISABLED_UpdateOptInStatus_OptedInUpdateFailed
#else
#define MAYBE_UpdateOptInStatus_OptedInUpdateFailed \
  UpdateOptInStatus_OptedInUpdateFailed
#endif
TEST_F(FastPairRepositoryImplTest,
       MAYBE_UpdateOptInStatus_OptedInUpdateFailed) {
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
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_IN, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  AccountKeyFilter filter(kFilterBytes1, {salt});
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata metadata(response, gfx::Image());

  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  base::RunLoop().RunUntilIdle();

  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_IN, status_);
  EXPECT_EQ(1u, devices_.size());
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_OptedOut) {
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_OPTED_OUT, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_OPTED_OUT, status_);
  EXPECT_EQ(0u, devices_.size());
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_OptStatusUnknown) {
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
  fast_pair_repository_->UpdateOptInStatus(
      nearby::fastpair::OptInStatus::STATUS_UNKNOWN, base::DoNothing());
  base::RunLoop().RunUntilIdle();
  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::STATUS_UNKNOWN, status_);
  EXPECT_EQ(0u, devices_.size());
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 1);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
}

TEST_F(FastPairRepositoryImplTest, GetSavedDevices_MissingResponse) {
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 0);
  footprints_fetcher_->SetGetUserDevicesResponse(std::nullopt);
  fast_pair_repository_->GetSavedDevices(
      base::BindOnce(&FastPairRepositoryImplTest::GetSavedDevicesCallback,
                     weak_ptr_factory_.GetWeakPtr()));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(nearby::fastpair::OptInStatus::
                STATUS_ERROR_RETRIEVING_FROM_FOOTPRINTS_SERVER,
            status_);
  EXPECT_EQ(0u, devices_.size());
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/true, 0);
  histogram_tester().ExpectBucketCount(kSavedDeviceGetDevicesResultMetricName,
                                       /*success=*/false, 1);
}

TEST_F(FastPairRepositoryImplTest,
       IsAccountKeyPairedLocally_SavedLocallyNotPaired) {
  // Simulate a device already saved to the registry. A Fast Pair device can
  // be saved in the registry even if it is not paired locally because
  // the SavedDeviceRegistry  tracks devices that have been Fast paired in the
  // past.
  bool success = saved_device_registry_->SaveAccountAssociation(
      kTestClassicAddress1, kAccountKey1);
  EXPECT_TRUE(success);
  EXPECT_TRUE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));

  EXPECT_TRUE(fast_pair_repository_->IsAccountKeyPairedLocally(kAccountKey1));
  EXPECT_FALSE(fast_pair_repository_->IsAccountKeyPairedLocally(kAccountKey2));
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_IsAccountKeyPairedLocally_PairedNotSavedLocally \
  DISABLED_IsAccountKeyPairedLocally_PairedNotSavedLocally
#else
#define MAYBE_IsAccountKeyPairedLocally_PairedNotSavedLocally \
  IsAccountKeyPairedLocally_PairedNotSavedLocally
#endif
TEST_F(FastPairRepositoryImplTest,
       MAYBE_IsAccountKeyPairedLocally_PairedNotSavedLocally) {
  // Simulate a device saved to a user's account that matches a device paired
  // with devices |adapter_| is loaded with. In the init of the |adapter_|, we
  // mocked `GetDevices` with |device_list_|.
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_account_key(
      std::string(kAccountKey1.begin(), kAccountKey1.end()));
  device->set_sha256_account_key_public_address(
      GenerateSha256AccountKeyMacAddress(
          std::string(kAccountKey1.begin(), kAccountKey1.end()),
          kTestClassicAddress1));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  // We want to simulate the cache being updated when it is parsing a
  // NotDiscoverableAdv, which happens when it is checking an account key.
  AccountKeyFilter filter(kFilterBytes1, {salt});
  auto run_loop = base::RunLoop();
  fast_pair_repository_->CheckAccountKeys(
      filter, base::BindOnce(&FastPairRepositoryImplTest::VerifyAccountKeyCheck,
                             base::Unretained(this), run_loop.QuitClosure(),
                             /*expected_result=*/false));
  run_loop.Run();

  // At this point the cache will be updated with any devices Saved to
  // Footprints. We can continue now checking if it matches any paired devices.
  EXPECT_TRUE(fast_pair_repository_->IsAccountKeyPairedLocally(kAccountKey1));
  EXPECT_FALSE(fast_pair_repository_->IsAccountKeyPairedLocally(kAccountKey2));
}

TEST_F(FastPairRepositoryImplTest, IsDeviceSavedToAccount_Match) {
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_account_key(
      std::string(kAccountKey1.begin(), kAccountKey1.end()));
  device->set_sha256_account_key_public_address(
      GenerateSha256AccountKeyMacAddress(
          std::string(kAccountKey1.begin(), kAccountKey1.end()),
          kTestClassicAddress1));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress1,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest,
       IsDeviceSavedToAccount_MatchKnownAndroidValues) {
  // In this test, we use a known triple from Android to ensure this works
  // cross-platform.
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_account_key(Base64Decode(kBase64AccountKey));
  device->set_sha256_account_key_public_address(
      Base64Decode(kBase64ExpectedSha256Hash));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress3,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_IsDeviceSavedToAccount_IgnoreForgetPattern \
  DISABLED_IsDeviceSavedToAccount_IgnoreForgetPattern
#else
#define MAYBE_IsDeviceSavedToAccount_IgnoreForgetPattern \
  IsDeviceSavedToAccount_IgnoreForgetPattern
#endif
TEST_F(FastPairRepositoryImplTest,
       MAYBE_IsDeviceSavedToAccount_IgnoreForgetPattern) {
  // In this test, we use a known "marked as forgotten" hash
  // from Android. Note that Android also leaves the account key
  // blank; however, we add an account key here to test the logic
  // of detecting the Forget pattern in the hash.
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_account_key(Base64Decode(kBase64AccountKey));
  device->set_sha256_account_key_public_address(
      Base64Decode(kBase64ForgetPatternSha256Hash));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress3,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest, IsDeviceSavedToAccount_NoMatch) {
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_account_key(
      std::string(kAccountKey1.begin(), kAccountKey1.end()));
  device->set_sha256_account_key_public_address(
      GenerateSha256AccountKeyMacAddress(
          std::string(kAccountKey1.begin(), kAccountKey1.end()),
          kTestClassicAddress1));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress2,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest, IsDeviceSavedToAccount_MissingResponse) {
  footprints_fetcher_->SetGetUserDevicesResponse(std::nullopt);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress1,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest, IsDeviceSavedToAccount_MissingAccountKey) {
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_sha256_account_key_public_address(
      GenerateSha256AccountKeyMacAddress(
          std::string(kAccountKey1.begin(), kAccountKey1.end()),
          kTestClassicAddress1));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress1,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_IsDeviceSavedToAccount_MissingSha \
  DISABLED_IsDeviceSavedToAccount_MissingSha
#else
#define MAYBE_IsDeviceSavedToAccount_MissingSha \
  IsDeviceSavedToAccount_MissingSha
#endif
TEST_F(FastPairRepositoryImplTest, MAYBE_IsDeviceSavedToAccount_MissingSha) {
  nearby::fastpair::FastPairInfo info;
  auto* device = info.mutable_device();
  device->set_account_key(
      std::string(kAccountKey1.begin(), kAccountKey1.end()));
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress1,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest,
       IsDeviceSavedToAccount_MissingShaAccountKey) {
  nearby::fastpair::FastPairInfo info;
  nearby::fastpair::UserReadDevicesResponse response;
  *response.add_fast_pair_info() = info;
  footprints_fetcher_->SetGetUserDevicesResponse(response);

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(false))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress1,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest, IsHashCorrect) {
  nearby::fastpair::GetObservedDeviceResponse device;
  DeviceMetadata metadata(device, gfx::Image());
  std::string account_key = Base64Decode(kBase64AccountKey);
  std::vector<uint8_t> account_key_bytes(account_key.begin(),
                                         account_key.end());

  // FakeFootprintsFetcher APIs are actually synchronous.
  footprints_fetcher_->AddUserFastPairInfo(
      BuildFastPairInfo(kValidModelId, account_key_bytes, kTestClassicAddress3,
                        kDeviceDisplayName, &metadata),
      base::DoNothing());

  base::MockCallback<base::OnceCallback<void(bool)>> callback;
  EXPECT_CALL(callback, Run(testing::Eq(true))).Times(1);
  fast_pair_repository_->IsDeviceSavedToAccount(kTestClassicAddress3,
                                                callback.Get());

  base::RunLoop().RunUntilIdle();
}

TEST_F(FastPairRepositoryImplTest,
       WriteAccountAssociationToFootprints_RemoveDeviceFromPendingWriteStore) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  // After a successful Footprints write, pending writes list should be empty.
  ASSERT_EQ(0u, pending_write_store_->GetPendingWrites().size());
}

// TODO(crbug.com/40264951): Re-enable this test
#if defined(MEMORY_SANITIZER)
#define MAYBE_RetriesWriteDevice_AfterNetworkAvailable \
  DISABLED_RetriesWriteDevice_AfterNetworkAvailable
#else
#define MAYBE_RetriesWriteDevice_AfterNetworkAvailable \
  RetriesWriteDevice_AfterNetworkAvailable
#endif
TEST_F(FastPairRepositoryImplTest,
       MAYBE_RetriesWriteDevice_AfterNetworkAvailable) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairInitial);
  device->set_classic_address(kTestClassicAddress1);

  // Mock an error due to Network failure.
  footprints_fetcher_->SetAddUserFastPairInfoResult(false);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);

  base::RunLoop().RunUntilIdle();

  // The failed write should save as pending write.
  std::vector<PendingWriteStore::PendingWrite> pending_writes =
      pending_write_store_->GetPendingWrites();
  ASSERT_EQ(1u, pending_writes.size());
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));

  // Parse device account key from device fast pair info.
  const std::string& account_key_str =
      pending_writes[0].fast_pair_info.device().account_key();
  std::vector<uint8_t> account_key =
      std::vector<uint8_t>{account_key_str.begin(), account_key_str.end()};

  // Check that account key retrieved from PendingWrite is the same as passed
  // into PendingWrite.
  ASSERT_EQ(kAccountKey1, account_key);

  // Reconnect to the Network, but fail again because |footprints_fetcher_| is
  // still stubbed to fail.
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // The write should still be pending after a failed retry.
  ASSERT_EQ(1u, pending_write_store_->GetPendingWrites().size());
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));

  // Reconnect to the Network, but within the 1 minute timeout.
  footprints_fetcher_->SetAddUserFastPairInfoResult(true);
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // Since we don't try within 1 minute, the write should still be pending.
  ASSERT_EQ(1u, pending_write_store_->GetPendingWrites().size());
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));

  // Mock waiting out the 1 minute timeout.
  task_environment()->FastForwardBy(base::Minutes(1));
  base::RunLoop().RunUntilIdle();

  // Reconnect to the Network, but after the 1 minute timeout.
  footprints_fetcher_->SetAddUserFastPairInfoResult(true);
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // The write, after a successful retry, should no longer be pending.
  ASSERT_EQ(0u, pending_write_store_->GetPendingWrites().size());
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
}

TEST_F(FastPairRepositoryImplTest,
       RetryWriteRetroactivePair_DoesntRecordMetric) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairRetroactive);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);

  // Mock an error due to Network failure.
  footprints_fetcher_->SetAddUserFastPairInfoResult(false);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);

  base::RunLoop().RunUntilIdle();

  // The failed write should save as pending write.
  std::vector<PendingWriteStore::PendingWrite> pending_writes =
      pending_write_store_->GetPendingWrites();
  ASSERT_EQ(1u, pending_writes.size());
  ASSERT_FALSE(footprints_fetcher_->ContainsKey(kAccountKey1));

  // Mock waiting out the 1 minute timeout.
  task_environment()->FastForwardBy(base::Minutes(1));
  base::RunLoop().RunUntilIdle();

  // Reconnect to the Network, but after the 1 minute timeout.
  footprints_fetcher_->SetAddUserFastPairInfoResult(true);
  fast_pair_repository_->DefaultNetworkChanged(
      helper_.network_state_handler()->DefaultNetwork());
  base::RunLoop().RunUntilIdle();

  // The write, after a successful retry, should no longer be pending.
  ASSERT_EQ(0u, pending_write_store_->GetPendingWrites().size());
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));

  // A pending write for retroactive pairing does not log a success in the
  // metrics.
  EXPECT_EQ(histogram_tester().GetBucketCount(
                kRetroactiveSuccessFunnelMetric,
                FastPairRetroactiveSuccessFunnelEvent::kSaveComplete),
            0);
}

// `WriteAccountAssociationToFootprints()` previously wrote the association
// locally as well. This unit test ensures it does not anymore.
TEST_F(FastPairRepositoryImplTest,
       AccountAssociationWriteToFootprints_NoLocalWrite) {
  auto device = base::MakeRefCounted<Device>(kValidModelId, kTestBLEAddress,
                                             Protocol::kFastPairRetroactive);
  device->set_classic_address(kTestClassicAddress1);
  device->set_display_name(kDeviceDisplayName);
  fast_pair_repository_->WriteAccountAssociationToFootprints(device,
                                                             kAccountKey1);
  ASSERT_TRUE(footprints_fetcher_->ContainsKey(kAccountKey1));
  ASSERT_FALSE(
      saved_device_registry_->IsAccountKeySavedToRegistry(kAccountKey1));
}

}  // namespace quick_pair
}  // namespace ash
