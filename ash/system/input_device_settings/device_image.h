// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_H_

#include "ash/ash_export.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Represents a device image, including its visual representation (ImageSkia)
// and associated metadata (device key, data URI).
class ASH_EXPORT DeviceImage {
 public:
  DeviceImage();
  ~DeviceImage();
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;
  explicit DeviceImage(const std::string& device_key,
                       const gfx::ImageSkia& image);
  DeviceImage(const std::string& device_key, const std::string& data_url);

  std::string device_key() const { return device_key_; }
  std::string data_url() const { return data_url_; }
  gfx::ImageSkia gfx_image_skia() const { return image_; }
  bool IsValid() const;

 private:
  std::string device_key_;
  std::string data_url_;
  gfx::ImageSkia image_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_H_
