// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/device_image_downloader.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/system/input_device_settings/device_image.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
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

}  // namespace

DeviceImageDownloader::DeviceImageDownloader() = default;
DeviceImageDownloader::~DeviceImageDownloader() = default;

GURL DeviceImageDownloader::GetResourceUrlFromDeviceKey(
    const std::string& device_key,
    DeviceImageDestination destination) {
  CHECK(!device_key.empty());

  std::string formatted_key = GetDeviceKeyForMetadataRequest(device_key);
  std::replace(formatted_key.begin(), formatted_key.end(), ':', '_');

  // Format strings for building image URLs based on destination.
  // Example URLs:
  // - Settings image: gstatic/chromeos/peripherals/0111_185a_icon.png
  // - Notification image: gstatic/chromeos/peripherals/0111_185a.png
  const std::string kSettingsIconFormatStr = "%s%s_icon%s";
  const std::string kNotificationFormatStr = "%s%s%s";

  // Select the appropriate format string based on the image destination.
  const std::string format_str =
      destination == DeviceImageDestination::kSettings ? kSettingsIconFormatStr
                                                       : kNotificationFormatStr;
  const std::string url = base::StringPrintf(
      format_str.c_str(), kGstaticBaseURL, formatted_key.c_str(), kFileFormat);
  return GURL(url);
}

void DeviceImageDownloader::DownloadImage(
    const std::string& device_key,
    const AccountId& account_id,
    DeviceImageDestination destination,
    base::OnceCallback<void(const DeviceImage& image)> callback) {
  const auto url = GetResourceUrlFromDeviceKey(device_key, destination);
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
