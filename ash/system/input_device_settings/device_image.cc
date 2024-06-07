// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/device_image.h"

#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

DeviceImage::DeviceImage() = default;
DeviceImage::~DeviceImage() = default;

DeviceImage::DeviceImage(const std::string& device_key,
                         const std::string& data_url)
    : device_key_(device_key), data_url_(data_url) {}

DeviceImage::DeviceImage(const std::string& device_key,
                         const gfx::ImageSkia& image)
    : device_key_(device_key) {
  if (!image.isNull()) {
    const SkBitmap bitmap = *image.bitmap();
    data_url_ = webui::GetBitmapDataUrl(bitmap);
    // Convert image to a gfx::Image for display in notifications.
    image_ = gfx::Image::CreateFrom1xBitmap(bitmap);
  }
}

bool DeviceImage::IsValid() const {
  return !data_url_.empty() || !image_.IsEmpty();
}

}  // namespace ash
