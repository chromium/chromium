// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager_session_delegate_impl.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

// The max number of Sea Pen image files to keep in Sea Pen directory before
// adding a new file.
constexpr int kMaxSeaPenFiles = 11;

SeaPenWallpaperManager* g_instance = nullptr;

std::vector<uint32_t> GetImageIdsImpl(const base::FilePath& directory) {
  std::vector<std::pair<base::FilePath, base::Time>> jpg_paths_with_timestamp;

  base::FileEnumerator jpg_enumerator(directory,
                                      /*recursive=*/false,
                                      base::FileEnumerator::FILES, "*.jpg");
  for (base::FilePath jpg_path = jpg_enumerator.Next(); !jpg_path.empty();
       jpg_path = jpg_enumerator.Next()) {
    const base::FileEnumerator::FileInfo info = jpg_enumerator.GetInfo();
    jpg_paths_with_timestamp.emplace_back(jpg_path, info.GetLastModifiedTime());
  }

  base::ranges::sort(jpg_paths_with_timestamp, base::ranges::greater(),
                     &std::pair<base::FilePath, base::Time>::second);

  std::vector<base::FilePath> jpg_paths;
  jpg_paths.reserve(jpg_paths_with_timestamp.size());

  base::ranges::transform(jpg_paths_with_timestamp,
                          std::back_inserter(jpg_paths),
                          &std::pair<base::FilePath, base::Time>::first);

  return GetIdsFromFilePaths(jpg_paths);
}

void TouchFileImpl(const base::FilePath& file_path) {
  if (!base::TouchFile(file_path, /*last_accessed=*/base::Time::Now(),
                       /*last_modified=*/base::Time::Now())) {
    LOG(WARNING) << "Failed to update accessed and modified time";
  }
}

// Reads the image from the given `file_path` and returns data as string.
std::string GetStringContent(const base::FilePath& file_path) {
  DVLOG(3) << "SeaPen reading " << file_path.BaseName();
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
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
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

void OnDecodeImageData(
    SeaPenWallpaperManager::GetImageAndMetadataCallback callback,
    const std::string& json,
    const gfx::ImageSkia& image) {
  if (image.isNull()) {
    // Do not bother decoding image metadata if we were unable to decode the
    // image.
    LOG(WARNING) << "Unable to decode image";
    std::move(callback).Run(gfx::ImageSkia(), nullptr);
    return;
  }
  DecodeJsonMetadata(json, base::BindOnce(std::move(callback), image));
}

}  // namespace

SeaPenWallpaperManager::SeaPenWallpaperManager()
    : session_delegate_(
          std::make_unique<SeaPenWallpaperManagerSessionDelegateImpl>()),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
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

void SeaPenWallpaperManager::SaveSeaPenImage(
    const AccountId& account_id,
    const SeaPenImage& sea_pen_image,
    const personalization_app::mojom::SeaPenQueryPtr& query,
    SaveSeaPenImageCallback callback) {
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
      base::BindOnce(&GetImageIdsImpl,
                     session_delegate_->GetStorageDirectory(account_id)),
      std::move(callback));
}

void SeaPenWallpaperManager::TouchFile(const AccountId& account_id,
                                       const uint32_t image_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TouchFileImpl,
                                GetFilePathForImageId(account_id, image_id)));
}

void SeaPenWallpaperManager::GetTemplateIdFromFile(
    const AccountId& account_id,
    const uint32_t image_id,
    GetTemplateIdFromFileCallback callback) {
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetStringContent,
                     GetFilePathForImageId(account_id, image_id)),
      base::BindOnce(&SeaPenWallpaperManager::OnFileReadGetTemplateId,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
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

void SeaPenWallpaperManager::SetSessionDelegateForTesting(
    std::unique_ptr<SessionDelegate> session_delegate) {
  session_delegate_ = std::move(session_delegate);
}

base::FilePath SeaPenWallpaperManager::GetFilePathForImageId(
    const AccountId& account_id,
    const uint32_t image_id) const {
  return session_delegate_->GetStorageDirectory(account_id)
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
      base::BindOnce(OnDecodeImageData, std::move(callback),
                     ExtractDcDescriptionContents(data)),
      data_decoder::mojom::ImageCodec::kDefault, data);
}

void SeaPenWallpaperManager::OnFileReadGetTemplateId(
    GetTemplateIdFromFileCallback callback,
    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (data.empty()) {
    LOG(WARNING) << "Unable to read file";
    std::move(callback).Run(std::nullopt);
    return;
  }

  DecodeJsonMetadataGetTemplateId(ExtractDcDescriptionContents(data),
                                  std::move(callback));
}

}  // namespace ash
