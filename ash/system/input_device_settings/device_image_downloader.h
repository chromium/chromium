// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_DOWNLOADER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_DOWNLOADER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

class AccountId;

namespace ash {

class DeviceImage;

class ASH_EXPORT DeviceImageDownloader {
 public:
  DeviceImageDownloader();
  ~DeviceImageDownloader();
  DeviceImageDownloader(const DeviceImageDownloader&) = delete;
  DeviceImageDownloader& operator=(const DeviceImageDownloader&) = delete;

  void DownloadImage(
      const std::string& device_key,
      const AccountId& account_id,
      base::OnceCallback<void(const DeviceImage& image)> callback);

 private:
  void OnImageDownloaded(
      const std::string& device_key,
      base::OnceCallback<void(const DeviceImage& image)> callback,
      const gfx::ImageSkia& image);

  base::WeakPtrFactory<DeviceImageDownloader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_DEVICE_IMAGE_DOWNLOADER_H_
