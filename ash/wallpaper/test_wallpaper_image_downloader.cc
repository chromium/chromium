// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_image_downloader.h"

#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/account_id/account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

TestWallpaperImageDownloader::TestWallpaperImageDownloader() = default;

TestWallpaperImageDownloader::~TestWallpaperImageDownloader() = default;

void TestWallpaperImageDownloader::DownloadGooglePhotosImage(
    const GURL& url,
    const AccountId& account_id,
    const absl::optional<std::string>& access_token,
    ImageDownloader::DownloadCallback callback) const {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), image_generator_.Run()));
}

void TestWallpaperImageDownloader::DownloadBackdropImage(
    const GURL& url,
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback) const {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), image_generator_.Run()));
}

}  // namespace ash
