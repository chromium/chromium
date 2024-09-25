// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/device_image_downloader.h"

#include <algorithm>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/system/input_device_settings/device_image.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "base/strings/strcat.h"
#include "components/account_id/account_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace ash {

namespace {

inline constexpr char kGstaticBaseURL[] =
    "https://www.gstatic.com/chromeos/peripherals/";

inline constexpr char kFileFormat[] = ".png";

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
          email: "cros-device-enablement@google.com"
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

}  // namespace

DeviceImageDownloader::DeviceImageDownloader() = default;
DeviceImageDownloader::~DeviceImageDownloader() = default;

GURL DeviceImageDownloader::GetResourceUrlFromDeviceKey(
    const std::string& device_key,
    DeviceImageDestination destination) {
  CHECK(!device_key.empty());

  std::string formatted_key = GetDeviceKeyForMetadataRequest(device_key);
  std::ranges::replace(formatted_key, ':', '_');

  // Format strings for building image URLs based on destination.
  // Example URLs:
  // - Settings image: gstatic/chromeos/peripherals/0111_185a_icon.png
  // - Notification image: gstatic/chromeos/peripherals/0111_185a.png
  return GURL(base::StrCat(
      {kGstaticBaseURL, formatted_key,
       destination == DeviceImageDestination::kSettings ? "_icon" : "",
       kFileFormat}));
}

void DeviceImageDownloader::DownloadImage(
    const std::string& device_key,
    const AccountId& account_id,
    DeviceImageDestination destination,
    base::OnceCallback<void(const DeviceImage& image)> callback) {
  const auto url = GetResourceUrlFromDeviceKey(device_key, destination);
  if (!ImageDownloader::Get()) {
    std::move(callback).Run(DeviceImage());
    return;
  }
  ImageDownloader::Get()->Download(
      url, kTrafficAnnotation, account_id,
      base::BindOnce(&DeviceImageDownloader::OnImageDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), device_key,
                     std::move(callback)));
}

void DeviceImageDownloader::OnImageDownloaded(
    const std::string& device_key,
    base::OnceCallback<void(const DeviceImage& image)> callback,
    const gfx::ImageSkia& image) {
  std::move(callback).Run(DeviceImage(device_key, image));
}

}  // namespace ash
