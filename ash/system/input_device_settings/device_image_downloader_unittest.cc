// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/device_image_downloader.h"

#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class DeviceImageDownloaderTest : public testing::Test {
 public:
  DeviceImageDownloaderTest() = default;
  DeviceImageDownloaderTest(const DeviceImageDownloaderTest&) = delete;
  DeviceImageDownloaderTest& operator=(const DeviceImageDownloaderTest&) =
      delete;
  ~DeviceImageDownloaderTest() override = default;

 protected:
  std::unique_ptr<DeviceImageDownloader> downloader_;
};

TEST_F(DeviceImageDownloaderTest, DeviceResourceDestination) {
  const std::string test_device_key = "0000:0001";

  // Test for kNotification
  GURL notification_url = downloader_->GetResourceUrlFromDeviceKey(
      test_device_key, DeviceImageDestination::kNotification);
  std::string expected_notification_image_url =
      "https://www.gstatic.com/chromeos/peripherals/0000_0001.png";
  EXPECT_EQ(expected_notification_image_url, notification_url.spec());

  // Test for kSettings
  GURL settings_url = downloader_->GetResourceUrlFromDeviceKey(
      test_device_key, DeviceImageDestination::kSettings);
  std::string expected_settings_image_url =
      "https://www.gstatic.com/chromeos/peripherals/0000_0001_icon.png";
  EXPECT_EQ(expected_settings_image_url, settings_url.spec());
}

}  // namespace ash
