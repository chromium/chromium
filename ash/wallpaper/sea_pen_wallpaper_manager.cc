// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <string>
#include <utility>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/account_id/account_id.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"

namespace ash {

namespace {

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

}  // namespace

SeaPenWallpaperManager::SeaPenWallpaperManager(
    WallpaperFileManager* wallpaper_file_manager)
    : wallpaper_file_manager_(wallpaper_file_manager),
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

void SeaPenWallpaperManager::SetStorageDirectory(
    const base::FilePath& storage_directory) {
  storage_directory_ = storage_directory;
}

base::FilePath SeaPenWallpaperManager::GetFilePathForImageId(
    const AccountId& account_id,
    const uint32_t image_id) const {
  CHECK(account_id.HasAccountIdKey());
  CHECK(!storage_directory_.empty());
  return GetAccountSeaPenWallpaperDir(storage_directory_, account_id)
      .Append(base::NumberToString(image_id))
      .AddExtension(".jpg");
}

void SeaPenWallpaperManager::DecodeAndSaveSeaPenImage(
    const AccountId& account_id,
    const SeaPenImage& sea_pen_image,
    const personalization_app::mojom::SeaPenQueryPtr& query,
    DecodeAndSaveSeaPenImageCallback callback) {
  CHECK(!storage_directory_.empty());
  CHECK(account_id.HasAccountIdKey());
  image_util::DecodeImageData(
      base::BindOnce(&SeaPenWallpaperManager::SaveSeaPenImage,
                     weak_factory_.GetWeakPtr(), account_id, sea_pen_image.id,
                     query.Clone(), std::move(callback)),
      data_decoder::mojom::ImageCodec::kDefault, sea_pen_image.jpg_bytes);
}

void SeaPenWallpaperManager::DeleteSeaPenImage(
    const AccountId& account_id,
    const uint32_t image_id,
    DeleteRecentSeaPenImageCallback callback) {
  wallpaper_file_manager_->RemoveImageFromDisk(
      std::move(callback), GetFilePathForImageId(account_id, image_id));
}

void SeaPenWallpaperManager::GetImageIds(const AccountId& account_id,
                                         GetImageIdsCallback callback) {
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetImageIdsImpl, storage_directory_, account_id),
      std::move(callback));
}

void SeaPenWallpaperManager::GetImageAndMetadata(
    const AccountId& account_id,
    const uint32_t image_id,
    GetImageAndMetadataCallback callback) {
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
  GetImageAndMetadata(account_id, image_id,
                      base::BindOnce(&DropImageInfo).Then(std::move(callback)));
}

void SeaPenWallpaperManager::SaveSeaPenImage(
    const AccountId& account_id,
    const uint32_t image_id,
    const personalization_app::mojom::SeaPenQueryPtr& query,
    DecodeAndSaveSeaPenImageCallback callback,
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull()) {
    LOG(ERROR) << __func__ << "Failed to decode Sea Pen image";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  DVLOG(2) << __func__ << " image_skia.size()=" << image_skia.size().ToString();
  const base::FilePath file_path = GetFilePathForImageId(account_id, image_id);
  const std::string metadata = QueryDictToXmpString(SeaPenQueryToDict(query));
  auto on_saved = base::BindOnce(&SeaPenWallpaperManager::OnSeaPenImageSaved,
                                 weak_factory_.GetWeakPtr(), image_skia,
                                 std::move(callback));
  wallpaper_file_manager_->SaveWallpaperToDisk(
      WallpaperType::kSeaPen, file_path.DirName(), file_path.BaseName().value(),
      WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED, image_skia, metadata,
      std::move(on_saved));
}

void SeaPenWallpaperManager::OnSeaPenImageSaved(
    const gfx::ImageSkia& image_skia,
    DecodeAndSaveSeaPenImageCallback callback,
    const base::FilePath& file_path) {
  if (file_path.empty()) {
    LOG(ERROR) << __func__ << "Failed to save Sea Pen image into disk";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  std::move(callback).Run(image_skia);
}

void SeaPenWallpaperManager::OnFileRead(GetImageAndMetadataCallback callback,
                                        const std::string data) {
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
