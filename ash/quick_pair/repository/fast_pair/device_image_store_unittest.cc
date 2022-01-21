// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_image_store.h"

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chromeos/services/bluetooth_config/public/cpp/device_image_info.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

constexpr char kTestModelId[] = "ABC123";

}  // namespace

namespace ash {
namespace quick_pair {

// Alias DeviceImageInfo for convenience.
using chromeos::bluetooth_config::DeviceImageInfo;

class DeviceImageStoreTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    nearby::fastpair::GetObservedDeviceResponse response;
    test_image_ = gfx::test::CreateImage(100, 100);
    device_metadata_ =
        std::make_unique<DeviceMetadata>(std::move(response), test_image_);

    device_image_store_ = std::make_unique<DeviceImageStore>();
  }

 protected:
  std::unique_ptr<DeviceMetadata> device_metadata_;
  gfx::Image test_image_;
  std::unique_ptr<DeviceImageStore> device_image_store_;
};

TEST_F(DeviceImageStoreTest, SaveDeviceImagesValid) {
  base::MockCallback<
      base::OnceCallback<void(DeviceImageStore::SaveDeviceImagesResult)>>
      callback;
  EXPECT_CALL(callback,
              Run(DeviceImageStore::SaveDeviceImagesResult::kSuccess));

  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        callback.Get());
}

TEST_F(DeviceImageStoreTest, SaveDeviceImagesInvalidDeviceImage) {
  base::MockCallback<
      base::OnceCallback<void(DeviceImageStore::SaveDeviceImagesResult)>>
      callback;
  EXPECT_CALL(callback,
              Run(DeviceImageStore::SaveDeviceImagesResult::kFailure));

  nearby::fastpair::GetObservedDeviceResponse response;
  DeviceMetadata empty_image_metadata =
      DeviceMetadata(std::move(response), gfx::Image());

  device_image_store_->SaveDeviceImages(kTestModelId, &empty_image_metadata,
                                        callback.Get());
}

TEST_F(DeviceImageStoreTest, PersistDeviceImagesValid) {
  // First, save the device images to memory.
  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        base::DoNothing());
  EXPECT_TRUE(device_image_store_->PersistDeviceImages(kTestModelId));

  // Validate that the images are persisted to prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value* device_image_store_dict =
      local_state->GetDictionary(DeviceImageStore::kDeviceImageStorePref);
  EXPECT_TRUE(device_image_store_dict);
  const base::Value* images_dict =
      device_image_store_dict->FindKey(kTestModelId);
  EXPECT_TRUE(images_dict);
  const std::string* persisted_image = images_dict->FindStringKey("Default");
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
  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        base::DoNothing());
  EXPECT_TRUE(device_image_store_->PersistDeviceImages(kTestModelId));
  EXPECT_TRUE(device_image_store_->EvictDeviceImages(kTestModelId));

  // Validate that the images are evicted from prefs.
  PrefService* local_state = Shell::Get()->local_state();
  const base::Value* device_image_store_dict =
      local_state->GetDictionary(DeviceImageStore::kDeviceImageStorePref);
  EXPECT_TRUE(device_image_store_dict);
  const base::Value* images_dict =
      device_image_store_dict->FindKey(kTestModelId);
  EXPECT_FALSE(images_dict);
}

TEST_F(DeviceImageStoreTest, EvictDeviceImagesInvalidModelId) {
  // Don't persist the device images to disk.
  EXPECT_FALSE(device_image_store_->EvictDeviceImages(kTestModelId));
}

TEST_F(DeviceImageStoreTest, EvictDeviceImagesInvalidDoubleFree) {
  // First, persist the device images to disk.
  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        base::DoNothing());
  EXPECT_TRUE(device_image_store_->PersistDeviceImages(kTestModelId));
  EXPECT_TRUE(device_image_store_->EvictDeviceImages(kTestModelId));

  // The second evict should fail.
  EXPECT_FALSE(device_image_store_->EvictDeviceImages(kTestModelId));
}

TEST_F(DeviceImageStoreTest, GetImagesForDeviceModelValid) {
  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        base::DoNothing());

  absl::optional<DeviceImageInfo> images =
      device_image_store_->GetImagesForDeviceModel(kTestModelId);
  EXPECT_TRUE(images);

  const std::string default_image = images->default_image();
  EXPECT_FALSE(default_image.empty());

  std::string expected_encoded_image =
      webui::GetBitmapDataUrl(test_image_.AsBitmap());
  EXPECT_EQ(default_image, expected_encoded_image);
}

TEST_F(DeviceImageStoreTest, GetImagesForDeviceModelInvalidUninitialized) {
  // Don't initialize the dictionary with any results.
  absl::optional<DeviceImageInfo> images =
      device_image_store_->GetImagesForDeviceModel(kTestModelId);
  EXPECT_FALSE(images);
}

TEST_F(DeviceImageStoreTest, GetImagesForDeviceModelInvalidNotAdded) {
  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        base::DoNothing());
  // Look for a model ID that wasn't added.
  absl::optional<DeviceImageInfo> images =
      device_image_store_->GetImagesForDeviceModel("DEF456");
  EXPECT_FALSE(images);
}

TEST_F(DeviceImageStoreTest, LoadPersistedImagesFromPrefs) {
  // First, persist the device images to disk.
  device_image_store_->SaveDeviceImages(kTestModelId, device_metadata_.get(),
                                        base::DoNothing());
  device_image_store_->PersistDeviceImages(kTestModelId);

  // A new/restarted DeviceImageStore instance should load persisted images
  // from prefs.
  DeviceImageStore new_device_image_store;
  EXPECT_TRUE(new_device_image_store.GetImagesForDeviceModel(kTestModelId));
}

}  // namespace quick_pair
}  // namespace ash
