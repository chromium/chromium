// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_image_downloader.h"

#include <optional>
#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Downloading from the internet will create a different ImageSkia each time.
// To simulate this same behavior, which WallpaperControllerImpl relies upon,
// use a RepeatingClosure to generate a new image for each download. Returns a
// high resolution image to ensure image resizing flow triggers.
gfx::ImageSkia CreateTestImage(const GURL&) {
  return gfx::test::CreateImageSkia(/*size=*/3000);
}

}  // namespace

TestWallpaperImageDownloader::TestWallpaperImageDownloader()
    : image_generator_(base::BindRepeating(&CreateTestImage)) {}

TestWallpaperImageDownloader::~TestWallpaperImageDownloader() = default;

void TestWallpaperImageDownloader::DownloadGooglePhotosImage(
    const GURL& url,
    const AccountId& account_id,
    const std::optional<std::string>& access_token,
    ImageDownloader::DownloadCallback callback) const {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), image_generator_.Run(url)));
}

void TestWallpaperImageDownloader::DownloadBackdropImage(
    const GURL& url,
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback) const {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), image_generator_.Run(url)));
}

}  // namespace ash
