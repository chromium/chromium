// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata_manager.h"

#include <memory>

#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/system/input_device_settings/device_image_downloader.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"

namespace ash {

namespace {

const std::string test_device_key = "0000:0001";
// Based on the default ImageSkia produced by `TestImageDownloader`.
constexpr char kExpectedDataUri[] =
    "data:image/"
    "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAAAAUCAIAAAA7jDsBAAAAF0lEQVQokWNk+M+"
    "ABzDhkxyVHpUmRRoAmpABJ+eiyP8AAAAASUVORK5CYII=";
}  // namespace

class InputDeviceSettingsMetadataManagerTest : public AshTestBase {
 public:
  InputDeviceSettingsMetadataManagerTest() = default;
  InputDeviceSettingsMetadataManagerTest(
      const InputDeviceSettingsMetadataManagerTest&) = delete;
  InputDeviceSettingsMetadataManagerTest& operator=(
      const InputDeviceSettingsMetadataManagerTest&) = delete;
  ~InputDeviceSettingsMetadataManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceSettingsMetadataManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsMetadataManager> manager_;
  TestImageDownloader image_downloader_;

  InputDeviceSettingsMetadataManager* manager() { return manager_.get(); }
};

TEST_F(InputDeviceSettingsMetadataManagerTest, GetDeviceImage) {
  base::RunLoop run_loop;
  manager()->GetDeviceImage(
      test_device_key,
      AccountId::FromUserEmailGaiaId("user@example.com", "123"),
      base::BindLambdaForTesting([&](const DeviceImage& device_image) {
        // Confirm that the image was encoded as a base64 data URL.
        EXPECT_TRUE(base::StartsWith(device_image.data_url(),
                                     "data:image/png;base64,"));
        EXPECT_EQ(test_device_key, device_image.device_key());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(InputDeviceSettingsMetadataManagerTest, DeviceImageCached) {
  manager()->GetDeviceImage(
      test_device_key,
      AccountId::FromUserEmailGaiaId("user@example.com", "123"),
      base::DoNothing());
  base::RunLoop().RunUntilIdle();
  const auto data_url = manager()->GetCachedDeviceImageDataUri(test_device_key);
  EXPECT_TRUE(data_url.has_value());
  EXPECT_EQ(kExpectedDataUri, data_url.value());
}

}  // namespace ash
