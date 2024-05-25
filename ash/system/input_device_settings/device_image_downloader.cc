// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/device_image_downloader.h"

#include "ash/public/cpp/image_downloader.h"
#include "ash/system/input_device_settings/device_image.h"
#include "components/account_id/account_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("device_image_downloader",
                                        R"(
  semantics {
    sender: "ChromeOS Welcome Experience"
    description:
      "Retrieves device images for use in notifications and "
      "display within device settings. Given a device key, "
      "Google's servers will return the image data in bytes, "
      "which is then decoded for use."
    trigger:
      "Triggered when a new input device is connected."
    data:
      "A device_key in the format <vid>:<pid> "
      "(where VID = vendor ID and PID = product ID) is "
      "used to specify the device image to fetch."
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts {
          email: "cros-peripherals@google.com"
      }
    }
    user_data {
      type: DEVICE_ID
    }
    last_reviewed: "2024-05-24"
  }
  policy {
    cookies_allowed: NO
    setting:
      "This feature is off by default and can be overridden by user."
    policy_exception_justification:
      "No content is uploaded or saved, this request downloads a "
      "publicly available image."
  }
)");

// TODO(b/329686601): Implement this.
GURL GetResourceUrlFromDeviceKey(const std::string& device_key) {
  return GURL();
}

}  // namespace

void DeviceImageDownloader::DownloadImage(
    const std::string& device_key,
    const AccountId& account_id,
    base::OnceCallback<void(const DeviceImage& image)> callback) {
  const auto url = GetResourceUrlFromDeviceKey(device_key);
  ImageDownloader::Get()->Download(
      url, kTrafficAnnotation, account_id,
      base::BindOnce(&DeviceImageDownloader::OnImageDownloaded,
                     base::Unretained(this), device_key, std::move(callback)));
}

// TODO(b/329686601): Store image as data URL in local state.
// TODO(b/329686601): Implement error handling for cases where the image
// download fails.
void DeviceImageDownloader::OnImageDownloaded(
    const std::string& device_key,
    base::OnceCallback<void(const DeviceImage& image)> callback,
    const gfx::ImageSkia& image) {
  std::move(callback).Run(DeviceImage(device_key, image));
}

}  // namespace ash
