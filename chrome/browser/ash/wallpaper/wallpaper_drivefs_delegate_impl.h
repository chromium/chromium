// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_IMPL_H_

#include "ash/public/cpp/wallpaper/wallpaper_drivefs_delegate.h"

#include <string>

#include "ash/public/cpp/image_downloader.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/account_id/account_id.h"
#include "components/drive/file_errors.h"
#include "google_apis/common/api_error_codes.h"
#include "url/gurl.h"

namespace ash {

class WallpaperDriveFsDelegateImpl : public WallpaperDriveFsDelegate {
 public:
  WallpaperDriveFsDelegateImpl();

  WallpaperDriveFsDelegateImpl(const WallpaperDriveFsDelegateImpl&) = delete;
  WallpaperDriveFsDelegateImpl& operator=(const WallpaperDriveFsDelegateImpl&) =
      delete;

  ~WallpaperDriveFsDelegateImpl() override;

  // WallpaperDriveFsDelegate:
  base::FilePath GetWallpaperPath(const AccountId& account_id) override;
  void SaveWallpaper(const AccountId& account_id,
                     const base::FilePath& source,
                     base::OnceCallback<void(bool success)> callback) override;
  void GetWallpaperModificationTime(
      const AccountId& account_id,
      base::OnceCallback<void(base::Time modification_time)> callback) override;
  void DownloadAndDecodeWallpaper(
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback) override;

 private:
  // Called when DriveFS has replied with file metadata that contains a URL to
  // download the wallpaper file. Actually downloading the wallpaper file still
  // requires an authentication token from Drive.
  void OnGetDownloadUrlMetadata(const AccountId& account_id,
                                ImageDownloader::DownloadCallback callback,
                                drive::FileError error,
                                drivefs::mojom::FileMetadataPtr metadata);

  // Called after `OnGetDownloadUrlMetadata` and after attempting to obtain a
  // Drive authentication token. Can now attempt to download the wallpaper image
  // because `download_url` and `authentication_token` are both present.
  void OnGetDownloadUrlAndAuthentication(
      ImageDownloader::DownloadCallback callback,
      const GURL& download_url,
      google_apis::ApiErrorCode error_code,
      const std::string& authentication_token);

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::WeakPtrFactory<WallpaperDriveFsDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WALLPAPER_WALLPAPER_DRIVEFS_DELEGATE_IMPL_H_
