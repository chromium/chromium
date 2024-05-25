// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_DOWNLOADER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_DOWNLOADER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "ui/gfx/image/image_skia.h"

class AccountId;

namespace ash {

class DeviceImage;

class ASH_EXPORT DeviceImageDownloader {
 public:
  void DownloadImage(
      const std::string& device_key,
      const AccountId& account_id,
      base::OnceCallback<void(const DeviceImage& image)> callback);

 private:
  void OnImageDownloaded(
      const std::string& device_key,
      base::OnceCallback<void(const DeviceImage& image)> callback,
      const gfx::ImageSkia& image);
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_DOWNLOADER_H_
