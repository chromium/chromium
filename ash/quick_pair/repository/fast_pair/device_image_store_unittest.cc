// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_image_store.h"

#include <optional>

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair/mock_fast_pair_image_decoder.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash::quick_pair {

namespace {

using ::base::test::RunOnceCallback;
using bluetooth_config::DeviceImageInfo;
using ::testing::_;

constexpr char kTestModelId[] = "ABC123";
constexpr char kTestLeftBudUrl[] = "left_bud";
constexpr char kTestRightBudUrl[] = "right_bud";
constexpr char kTestCaseUrl[] = "case";

}  // namespace

class DeviceImageStoreTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    nearby::fastpair::TrueWirelessHeadsetImages true_wireless_images;
    true_wireless_images.set_left_bud_url(kTestLeftBudUrl);
    true_wireless_images.set_right_bud_url(kTestRightBudUrl);
    true_wireless_images.set_case_url(kTestCaseUrl);

    nearby::fastpair::Device device;
    *device.mutable_true_wireless_images() = true_wireless_images;

    nearby::fastpair::GetObservedDeviceResponse response;
    *response.mutable_device() = device;

    test_image_ = gfx::test::CreateImage(100, 100);
    device_metadata_ = std::make_unique<DeviceMetadata>(response, test_image_);

    mock_decoder_ = std::make_unique<MockFastPairImageDecoder>();
    // On call to DecodeImage, run the third argument callback with test_image_.
    ON_CALL(*mock_decoder_, DecodeImageFromUrl(_, _, _))
        .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>(test_image_));

    device_image_store_ =
        std::make_unique<DeviceImageStore>(mock_decoder_.get());
  }

 protected:
  std::unique_ptr<DeviceMetadata> device_metadata_;
  gfx::Image test_image_;
  std::unique_ptr<MockFastPairImageDecoder> mock_decoder_;
  std::unique_ptr<DeviceImageStore> device_image_store_;
};

TEST_F(DeviceImageStoreTest, FetchDeviceImagesValidDefaultOnly) {
  base::MockCallback<DeviceImageStore::FetchDeviceImagesCallback> callback;
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kDefault,
                         DeviceImageStore::FetchDeviceImagesResult::kSuccess)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kLeftBud,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kRightBud,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kCase,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);

  // Only include the default image in the metadata.
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata default_only_metadata =
      DeviceMetadata(std::move(response), test_image_);
  device_image_store_->FetchDeviceImages(kTestModelId, &default_only_metadata,
                                         callback.Get());
}

TEST_F(DeviceImageStoreTest, FetchDeviceImagesInvalidDefaultOnly) {
  base::MockCallback<DeviceImageStore::FetchDeviceImagesCallback> callback;
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kDefault,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kLeftBud,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kRightBud,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kCase,
                         DeviceImageStore::FetchDeviceImagesResult::kSkipped)))
      .Times(1);

  // Include only an empty default image in the metadata.
  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata empty_image_metadata =
      DeviceMetadata(std::move(response), gfx::Image());
  device_image_store_->FetchDeviceImages(kTestModelId, &empty_image_metadata,
                                         callback.Get());
}

TEST_F(DeviceImageStoreTest, FetchDeviceImagesValidTrueWireless) {
  base::MockCallback<DeviceImageStore::FetchDeviceImagesCallback> callback;
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kDefault,
                         DeviceImageStore::FetchDeviceImagesResult::kSuccess)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kLeftBud,
                         DeviceImageStore::FetchDeviceImagesResult::kSuccess)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kRightBud,
                         DeviceImageStore::FetchDeviceImagesResult::kSuccess)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kCase,
                         DeviceImageStore::FetchDeviceImagesResult::kSuccess)))
      .Times(1);

  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         callback.Get());
}

TEST_F(DeviceImageStoreTest, FetchDeviceImagesInvalidTrueWireless) {
  base::MockCallback<DeviceImageStore::FetchDeviceImagesCallback> callback;
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kDefault,
                         DeviceImageStore::FetchDeviceImagesResult::kSuccess)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kLeftBud,
                         DeviceImageStore::FetchDeviceImagesResult::kFailure)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kRightBud,
                         DeviceImageStore::FetchDeviceImagesResult::kFailure)))
      .Times(1);
  EXPECT_CALL(
      callback,
      Run(std::make_pair(DeviceImageStore::DeviceImageType::kCase,
                         DeviceImageStore::FetchDeviceImagesResult::kFailure)))
      .Times(1);

  // Simulate an error during download/decode by returning an empty image.
  ON_CALL(*mock_decoder_, DecodeImageFromUrl(_, _, _))
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<2>(gfx::Image()));
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         callback.Get());
}

TEST_F(DeviceImageStoreTest, PersistDeviceImagesValid) {
  // First, save the device images to memory.
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         base::DoNothing());
  EXPECT_TRUE(device_image_store_->PersistDeviceImages(kTestModelId));

  // Validate that the images are persisted to prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_image_store_dict =
      local_state->GetDict(DeviceImageStore::kDeviceImageStorePref);
  const base::Value::Dict* images_dict =
      device_image_store_dict.FindDict(kTestModelId);
  EXPECT_TRUE(images_dict);
  const std::string* persisted_image = images_dict->FindString("Default");
  std::string expected_encoded_image =
      webui::GetBitmapDataUrl(test_image_.AsBitmap());
  EXPECT_EQ(*persisted_image, expected_encoded_image);
}

TEST_F(DeviceImageStoreTest, PersistDeviceImagesInvalidModelId) {
  // Don't save the device images to memory.
  EXPECT_FALSE(device_image_store_->PersistDeviceImages(kTestModelId));
}

TEST_F(DeviceImageStoreTest, EvictDeviceImagesValid) {
  // First, persist the device images to disk.
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         base::DoNothing());
  EXPECT_TRUE(device_image_store_->PersistDeviceImages(kTestModelId));
  EXPECT_TRUE(device_image_store_->EvictDeviceImages(kTestModelId));

  // Validate that the images are evicted from prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value::Dict& device_image_store_dict =
      local_state->GetDict(DeviceImageStore::kDeviceImageStorePref);
  const base::Value* images_dict = device_image_store_dict.Find(kTestModelId);
  EXPECT_FALSE(images_dict);
}

TEST_F(DeviceImageStoreTest, EvictDeviceImagesInvalidModelId) {
  // Don't persist the device images to disk.
  EXPECT_FALSE(device_image_store_->EvictDeviceImages(kTestModelId));
}

TEST_F(DeviceImageStoreTest, EvictDeviceImagesInvalidDoubleFree) {
  // First, persist the device images to disk.
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         base::DoNothing());
  EXPECT_TRUE(device_image_store_->PersistDeviceImages(kTestModelId));
  EXPECT_TRUE(device_image_store_->EvictDeviceImages(kTestModelId));

  // The second evict should fail.
  EXPECT_FALSE(device_image_store_->EvictDeviceImages(kTestModelId));
}

TEST_F(DeviceImageStoreTest, GetImagesForDeviceModelValid) {
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         base::DoNothing());

  std::optional<DeviceImageInfo> images =
      device_image_store_->GetImagesForDeviceModel(kTestModelId);
  EXPECT_TRUE(images);

  const std::string default_image = images->default_image();
  EXPECT_FALSE(default_image.empty());
  EXPECT_EQ(default_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));

  const std::string left_bud_image = images->left_bud_image();
  EXPECT_FALSE(left_bud_image.empty());
  EXPECT_EQ(left_bud_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));

  const std::string right_bud_image = images->right_bud_image();
  EXPECT_FALSE(right_bud_image.empty());
  EXPECT_EQ(right_bud_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));

  const std::string case_image = images->case_image();
  EXPECT_FALSE(case_image.empty());
  EXPECT_EQ(case_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));
}

TEST_F(DeviceImageStoreTest, GetImagesForDeviceModelInvalidUninitialized) {
  // Don't initialize the dictionary with any results.
  std::optional<DeviceImageInfo> images =
      device_image_store_->GetImagesForDeviceModel(kTestModelId);
  EXPECT_FALSE(images);
}

TEST_F(DeviceImageStoreTest, GetImagesForDeviceModelInvalidNotAdded) {
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         base::DoNothing());
  // Look for a model ID that wasn't added.
  std::optional<DeviceImageInfo> images =
      device_image_store_->GetImagesForDeviceModel("DEF456");
  EXPECT_FALSE(images);
}

TEST_F(DeviceImageStoreTest, LoadPersistedImagesFromPrefs) {
  // First, persist the device images to disk.
  device_image_store_->FetchDeviceImages(kTestModelId, device_metadata_.get(),
                                         base::DoNothing());
  device_image_store_->PersistDeviceImages(kTestModelId);

  // A new/restarted DeviceImageStore instance should load persisted images
  // from prefs.
  DeviceImageStore new_device_image_store(mock_decoder_.get());
  std::optional<DeviceImageInfo> images =
      new_device_image_store.GetImagesForDeviceModel(kTestModelId);
  EXPECT_TRUE(images);

  const std::string default_image = images->default_image();
  EXPECT_FALSE(default_image.empty());
  EXPECT_EQ(default_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));

  const std::string left_bud_image = images->left_bud_image();
  EXPECT_FALSE(left_bud_image.empty());
  EXPECT_EQ(left_bud_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));

  const std::string right_bud_image = images->right_bud_image();
  EXPECT_FALSE(right_bud_image.empty());
  EXPECT_EQ(right_bud_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));

  const std::string case_image = images->case_image();
  EXPECT_FALSE(case_image.empty());
  EXPECT_EQ(case_image, webui::GetBitmapDataUrl(test_image_.AsBitmap()));
}

}  // namespace ash::quick_pair
