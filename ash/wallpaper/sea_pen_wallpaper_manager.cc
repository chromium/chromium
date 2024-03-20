// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <string>
#include <utility>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/account_id/account_id.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// The max number of Sea Pen image files to keep in Sea Pen directory before
// adding a new file.
constexpr int kMaxSeaPenFiles = 11;

SeaPenWallpaperManager* g_instance = nullptr;

base::FilePath GetAccountSeaPenWallpaperDir(
    const base::FilePath& storage_directory,
    const AccountId& account_id) {
  return storage_directory.Append(account_id.GetAccountIdKey());
}

std::vector<uint32_t> GetImageIdsImpl(const base::FilePath& storage_directory,
                                      const AccountId& account_id) {
  std::vector<base::FilePath> jpg_paths;

  base::FileEnumerator jpg_enumerator(
      GetAccountSeaPenWallpaperDir(storage_directory, account_id),
      /*recursive=*/false, base::FileEnumerator::FILES, "*.jpg");
  for (base::FilePath jpg_path = jpg_enumerator.Next(); !jpg_path.empty();
       jpg_path = jpg_enumerator.Next()) {
    jpg_paths.push_back(jpg_path);
  }

  return GetIdsFromFilePaths(jpg_paths);
}

// Reads the image from the given `file_path` and returns data as string.
std::string GetStringContent(const base::FilePath& file_path) {
  if (file_path.empty() || !base::PathExists(file_path)) {
    LOG(WARNING) << "File path is empty or does not exist";
    return std::string();
  }

  std::string result;
  if (!base::ReadFileToString(file_path, &result)) {
    LOG(WARNING) << "Failed reading file";
    result.clear();
  }

  return result;
}

gfx::ImageSkia DropImageInfo(
    const gfx::ImageSkia& image,
    personalization_app::mojom::RecentSeaPenImageInfoPtr info) {
  return image;
}

// Deletes `file_path` from disk. Returns true if the file did exist and was
// deleted. Must only be called with paths ending in ".jpg".
bool DeleteJpgFromDisk(const base::FilePath& file_path) {
  DCHECK_EQ(".jpg", file_path.Extension());
  if (base::PathExists(file_path)) {
    return base::DeleteFile(file_path);
  }
  return false;
}

// Scans through all the images in Sea Pen wallpaper directory. Keeps only 9
// latest sea pen images based on the last modified time, the older files are
// removed.
void MaybeDeleteOldSeaPenImages(const base::FilePath& wallpaper_dir) {
  std::vector<std::pair<base::FilePath, base::Time>> sea_pen_files;

  // Enumerate normal files only; directories and symlinks are skipped.
  base::FileEnumerator enumerator(wallpaper_dir, true,
                                  base::FileEnumerator::FILES);
  for (base::FilePath file_path = enumerator.Next(); !file_path.empty();
       file_path = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    sea_pen_files.emplace_back(file_path, info.GetLastModifiedTime());
  }

  if (sea_pen_files.size() <= kMaxSeaPenFiles) {
    return;
  }

  // Finds the n oldest files (n = total files - kMaxSeaPenFiles) then resizes
  // sea_pen_files to store only the old files.
  std::nth_element(sea_pen_files.begin(), sea_pen_files.end() - kMaxSeaPenFiles,
                   sea_pen_files.end(),
                   [](const auto& left, const auto& right) {
                     return left.second < right.second;
                   });
  sea_pen_files.resize(sea_pen_files.size() - kMaxSeaPenFiles);

  // Removes all the old images.
  for (const auto& [file_path, _] : sea_pen_files) {
    if (!DeleteJpgFromDisk(file_path)) {
      LOG(ERROR) << __func__ << " failed to remove old Sea Pen file";
      return;
    }
  }
}

}  // namespace

SeaPenWallpaperManager::SeaPenWallpaperManager()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

SeaPenWallpaperManager::~SeaPenWallpaperManager() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
SeaPenWallpaperManager* SeaPenWallpaperManager::GetInstance() {
  return g_instance;
}

void SeaPenWallpaperManager::SetStorageDirectory(
    const base::FilePath& storage_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  storage_directory_ = storage_directory;
}

void SeaPenWallpaperManager::SaveSeaPenImage(
    const AccountId& account_id,
    const SeaPenImage& sea_pen_image,
    const personalization_app::mojom::SeaPenQueryPtr& query,
    SaveSeaPenImageCallback callback) {
  CHECK(!storage_directory_.empty());
  CHECK(account_id.HasAccountIdKey());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  image_util::DecodeImageData(
      base::BindOnce(&SeaPenWallpaperManager::OnSeaPenImageDecoded,
                     weak_factory_.GetWeakPtr(), account_id, sea_pen_image.id,
                     query.Clone(), std::move(callback)),
      data_decoder::mojom::ImageCodec::kDefault, sea_pen_image.jpg_bytes);
}

void SeaPenWallpaperManager::DeleteSeaPenImage(
    const AccountId& account_id,
    const uint32_t image_id,
    DeleteRecentSeaPenImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DeleteJpgFromDisk,
                     GetFilePathForImageId(account_id, image_id)),
      std::move(callback));
}

void SeaPenWallpaperManager::GetImageIds(const AccountId& account_id,
                                         GetImageIdsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetImageIdsImpl, storage_directory_, account_id),
      std::move(callback));
}

void SeaPenWallpaperManager::GetImageAndMetadata(
    const AccountId& account_id,
    const uint32_t image_id,
    GetImageAndMetadataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetStringContent,
                     GetFilePathForImageId(account_id, image_id)),
      base::BindOnce(&SeaPenWallpaperManager::OnFileRead,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SeaPenWallpaperManager::GetImage(const AccountId& account_id,
                                      const uint32_t image_id,
                                      GetImageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetImageAndMetadata(account_id, image_id,
                      base::BindOnce(&DropImageInfo).Then(std::move(callback)));
}

base::FilePath SeaPenWallpaperManager::GetFilePathForImageId(
    const AccountId& account_id,
    const uint32_t image_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(account_id.HasAccountIdKey());
  CHECK(!storage_directory_.empty());
  return GetAccountSeaPenWallpaperDir(storage_directory_, account_id)
      .Append(base::NumberToString(image_id))
      .AddExtension(".jpg");
}

void SeaPenWallpaperManager::OnSeaPenImageDecoded(
    const AccountId& account_id,
    const uint32_t image_id,
    const personalization_app::mojom::SeaPenQueryPtr& query,
    SaveSeaPenImageCallback callback,
    const gfx::ImageSkia& image_skia) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (image_skia.isNull()) {
    LOG(ERROR) << __func__ << "Failed to decode Sea Pen image";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  DVLOG(2) << __func__ << " image_skia.size()=" << image_skia.size().ToString();

  const base::FilePath image_path = GetFilePathForImageId(account_id, image_id);

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateDirectoryAndLogError, image_path.DirName())
          .Then(
              base::BindOnce(&MaybeDeleteOldSeaPenImages, image_path.DirName()))
          .Then(base::BindOnce(&ResizeAndSaveWallpaper, image_skia, image_path,
                               WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED,
                               image_skia.size(),
                               QueryDictToXmpString(SeaPenQueryToDict(query)))),
      std::move(callback));
  }

void SeaPenWallpaperManager::OnFileRead(GetImageAndMetadataCallback callback,
                                        const std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data.empty()) {
    LOG(WARNING) << "Unable to read file";
    std::move(callback).Run(gfx::ImageSkia(), nullptr);
    return;
  }
  image_util::DecodeImageData(
      base::BindOnce(&SeaPenWallpaperManager::OnDecodeImageData,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     ExtractDcDescriptionContents(data)),
      data_decoder::mojom::ImageCodec::kDefault, data);
}

void SeaPenWallpaperManager::OnDecodeImageData(
    GetImageAndMetadataCallback callback,
    const std::string json,
    const gfx::ImageSkia& image) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (image.isNull()) {
    // Do not bother decoding image metadata if we were unable to decode the
    // image.
    LOG(WARNING) << "Unable to decode image";
    std::move(callback).Run(gfx::ImageSkia(), nullptr);
    return;
  }
  DecodeJsonMetadata(json, base::BindOnce(std::move(callback), image));
}

}  // namespace ash
