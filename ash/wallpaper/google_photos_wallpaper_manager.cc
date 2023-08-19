// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/google_photos_wallpaper_manager.h"

#include <memory>

#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

namespace ash {

namespace {
base::FilePath GetGooglePhotosWallpaperPath(const base::FilePath& wallpaper_dir,
                                            const std::string& photo_id) {
  DCHECK(!wallpaper_dir.empty());
  return wallpaper_dir.Append(photo_id);
}

// Creates the google_photos directory in the local file system for caching
// Google Photos wallpapers if it does not already exist.
void EnsureGooglePhotosDirectoryExists(const base::FilePath& wallpaper_dir) {
  if (!base::DirectoryExists(wallpaper_dir)) {
    base::CreateDirectory(wallpaper_dir);
  }
}

void DeleteGooglePhotosPath(const base::FilePath& wallpaper_dir) {
  base::DeletePathRecursively(wallpaper_dir);
}

}  // namespace

GooglePhotosWallpaperManager::GooglePhotosWallpaperManager(
    WallpaperImageDownloader* wallpaper_image_downloader)
    : wallpaper_image_downloader_(wallpaper_image_downloader),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

GooglePhotosWallpaperManager::~GooglePhotosWallpaperManager() = default;

void GooglePhotosWallpaperManager::SetClient(
    WallpaperControllerClient* client) {
  wallpaper_controller_client_ = client;
}

void GooglePhotosWallpaperManager::GetGooglePhotosWallpaper(
    const base::FilePath& wallpaper_dir,
    const GooglePhotosWallpaperParams& params,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    LoadGooglePhotosWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto file_path = GetGooglePhotosWallpaperPath(wallpaper_dir, photo->id);
  auto on_load = base::BindOnce(&GooglePhotosWallpaperManager::
                                    OnLoadExistingGooglePhotosWallpaperComplete,
                                weak_factory_.GetWeakPtr(), wallpaper_dir,
                                params, std::move(photo), std::move(callback));
  LoadGooglePhotosWallpaper(file_path, std::move(on_load));
}

void GooglePhotosWallpaperManager::LoadGooglePhotosWallpaper(
    const base::FilePath& file_path,
    LoadGooglePhotosWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, file_path),
      base::BindOnce(&GooglePhotosWallpaperManager::LoadFromDisk,
                     weak_factory_.GetWeakPtr(), file_path,
                     std::move(callback)));
}

void GooglePhotosWallpaperManager::LoadFromDisk(
    const base::FilePath& file_path,
    LoadGooglePhotosWallpaperCallback callback,
    bool file_path_exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!file_path_exists) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  image_util::DecodeImageFile(std::move(callback), file_path);
}

void GooglePhotosWallpaperManager::DownloadGooglePhotosWallpaper(
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    const AccountId& account_id,
    LoadGooglePhotosWallpaperCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wallpaper_controller_client_->FetchGooglePhotosAccessToken(
      account_id, base::BindOnce(&GooglePhotosWallpaperManager::
                                     OnGooglePhotosAuthenticationTokenFetched,
                                 weak_factory_.GetWeakPtr(), std::move(photo),
                                 account_id, std::move(callback)));
}

void GooglePhotosWallpaperManager::OnGooglePhotosAuthenticationTokenFetched(
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    const AccountId& account_id,
    LoadGooglePhotosWallpaperCallback callback,
    const absl::optional<std::string>& access_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wallpaper_image_downloader_->DownloadGooglePhotosImage(
      photo->url, account_id, access_token, std::move(callback));
}

void GooglePhotosWallpaperManager::OnGooglePhotosWallpaperDownloaded(
    const base::FilePath& wallpaper_dir,
    const std::string& photo_id,
    const WallpaperLayout layout,
    LoadGooglePhotosWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (image.isNull()) {
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  auto file_path = GetGooglePhotosWallpaperPath(wallpaper_dir, photo_id);
  // Clear persistent cache and repopulate with current Google Photos wallpaper.
  gfx::ImageSkia thread_safe_image(image);
  thread_safe_image.MakeThreadSafe();
  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteGooglePhotosPath, wallpaper_dir)
          .Then(
              base::BindOnce(&EnsureGooglePhotosDirectoryExists, wallpaper_dir))
          .Then(base::BindOnce(&ResizeAndSaveWallpaper, thread_safe_image,
                               file_path, layout, thread_safe_image.width(),
                               thread_safe_image.height())),
      base::BindOnce([](bool success) {
        if (!success) {
          LOG(ERROR) << "Failed to save Google Photos wallpaper.";
        }
      }));

  std::move(callback).Run(image);
}

void GooglePhotosWallpaperManager::OnLoadExistingGooglePhotosWallpaperComplete(
    const base::FilePath& wallpaper_dir,
    const GooglePhotosWallpaperParams& params,
    ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
    LoadGooglePhotosWallpaperCallback callback,
    const gfx::ImageSkia& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  if (image.isNull()) {
    auto on_download = base::BindOnce(
        &GooglePhotosWallpaperManager::OnGooglePhotosWallpaperDownloaded,
        weak_factory_.GetWeakPtr(), wallpaper_dir, photo->id, params.layout,
        std::move(callback));
    DownloadGooglePhotosWallpaper(std::move(photo), params.account_id,
                                  std::move(on_download));
  } else {
    std::move(callback).Run(image);
  }
}

}  // namespace ash
