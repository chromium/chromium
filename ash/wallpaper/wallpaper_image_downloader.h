// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_IMAGE_DOWNLOADER_H_
#define ASH_WALLPAPER_WALLPAPER_IMAGE_DOWNLOADER_H_

#include <optional>
#include <string>

#include "ash/public/cpp/image_downloader.h"

class AccountId;
class GURL;

namespace ash {

class WallpaperImageDownloader {
 public:
  virtual ~WallpaperImageDownloader() = default;

  virtual void DownloadGooglePhotosImage(
      const GURL& url,
      const AccountId& account_id,
      const std::optional<std::string>& access_token,
      ImageDownloader::DownloadCallback callback) const = 0;

  virtual void DownloadBackdropImage(
      const GURL& url,
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback) const = 0;
};

class WallpaperImageDownloaderImpl : public WallpaperImageDownloader {
 public:
  WallpaperImageDownloaderImpl();

  WallpaperImageDownloaderImpl(const WallpaperImageDownloaderImpl&) = delete;
  WallpaperImageDownloaderImpl& operator=(const WallpaperImageDownloaderImpl&) =
      delete;

  ~WallpaperImageDownloaderImpl() override;

  // WallpaperImageDownloader:
  void DownloadGooglePhotosImage(
      const GURL& photo,
      const AccountId& account_id,
      const std::optional<std::string>& access_token,
      ImageDownloader::DownloadCallback callback) const override;

  void DownloadBackdropImage(
      const GURL& url,
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback) const override;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_IMAGE_DOWNLOADER_H_
