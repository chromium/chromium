// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_TEST_WALLPAPER_IMAGE_DOWNLOADER_H_
#define ASH_WALLPAPER_TEST_WALLPAPER_IMAGE_DOWNLOADER_H_

#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "ash/wallpaper/wallpaper_image_downloader.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

class AccountId;
class GURL;

namespace ash {

class TestWallpaperImageDownloader : public WallpaperImageDownloader {
 public:
  TestWallpaperImageDownloader();

  TestWallpaperImageDownloader(const TestWallpaperImageDownloader&) = delete;
  TestWallpaperImageDownloader& operator=(const TestWallpaperImageDownloader&) =
      delete;

  ~TestWallpaperImageDownloader() override;

  using ImageGenerator = base::RepeatingCallback<gfx::ImageSkia()>;
  void set_image_generator(ImageGenerator image_generator) {
    image_generator_ = image_generator;
  }

  // WallpaperImageDownloader:
  void DownloadGooglePhotosImage(
      const GURL& url,
      const AccountId& account_id,
      const absl::optional<std::string>& access_token,
      ImageDownloader::DownloadCallback callback) const override;

  void DownloadBackdropImage(
      const GURL& url,
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback) const override;

 private:
  // Downloading from the internet will create a different ImageSkia each time.
  // To simulate this same behavior, which WallpaperControllerImpl relies upon,
  // use a RepeatingClosure to generate a new image for each download. Returns a
  // high resolution image to ensure image resizing flow triggers.
  ImageGenerator image_generator_ =
      base::BindRepeating(gfx::test::CreateImageSkia,
                          /*width=*/3000,
                          /*height=*/3000);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_TEST_WALLPAPER_IMAGE_DOWNLOADER_H_
