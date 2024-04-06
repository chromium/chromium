// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_drivefs_delegate.h"

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

TestWallpaperDriveFsDelegate::TestWallpaperDriveFsDelegate() = default;

TestWallpaperDriveFsDelegate::~TestWallpaperDriveFsDelegate() = default;

void TestWallpaperDriveFsDelegate::Reset() {
  save_wallpaper_account_id_.clear();
  get_wallpaper_path_account_id_.clear();
}

base::FilePath TestWallpaperDriveFsDelegate::GetWallpaperPath(
    const AccountId& account_id) {
  get_wallpaper_path_account_id_ = account_id;
  return base::FilePath();
}

void TestWallpaperDriveFsDelegate::SaveWallpaper(
    const AccountId& account_id,
    const base::FilePath& source,
    base::OnceCallback<void(bool)> callback) {
  save_wallpaper_account_id_ = account_id;
  std::move(callback).Run(true);
}

void TestWallpaperDriveFsDelegate::GetWallpaperModificationTime(
    const AccountId& account_id,
    GetWallpaperModificationTimeCallback callback) {
  std::move(callback).Run(base::Time::Now());
}

void TestWallpaperDriveFsDelegate::WaitForWallpaperChange(
    const AccountId& account_id,
    WaitForWallpaperChangeCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

void TestWallpaperDriveFsDelegate::DownloadAndDecodeWallpaper(
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback) {
  std::move(callback).Run(gfx::test::CreateImageSkia(10));
}

}  // namespace ash
