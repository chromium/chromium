// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

class AccountId;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

// A utility class to save / load / delete / enumerate SeaPen images on disk.
// Accessible via a singleton getter.
class ASH_EXPORT SeaPenWallpaperManager {
 public:
  explicit SeaPenWallpaperManager(WallpaperFileManager* wallpaper_file_manager);

  SeaPenWallpaperManager(const SeaPenWallpaperManager&) = delete;
  SeaPenWallpaperManager& operator=(const SeaPenWallpaperManager&) = delete;

  ~SeaPenWallpaperManager();

  // `SeaPenWallpaperManager` is owned by and has the same lifetime as
  // `WallpaperController`, so it should exist very early after `Shell` init and
  // last until `Shell` teardown.
  static SeaPenWallpaperManager* GetInstance();

  // Set the directory that stores SeaPen images. It is an error to call any
  // other method before calling `SetStorageDirectory` with a valid directory.
  // Stores images for all users, with subfolders keyed by AccountId. Example
  // output of `tree directory`:
  // -- g-<user-1-hash>
  //    |-- 12345.jpg
  //    |-- 23456.jpg
  // -- g-<user-2-hash>
  //    |-- 9876.jpg
  //    |-- 8765.jpg
  void SetStorageDirectory(const base::FilePath& storage_directory);

  // Get the full `FilePath` for the SeaPen image at `image_id`.
  base::FilePath GetFilePathForImageId(const AccountId& account_id,
                                       uint32_t image_id) const;

  using DecodeAndSaveSeaPenImageCallback =
      base::OnceCallback<void(const gfx::ImageSkia& image_skia)>;

  // Decodes Sea Pen image then save the decoded image into disk. Calls
  // `callback` with the decoded image. Responds with an empty `ImageSkia` on
  // decoding failure or file saving failure.
  void DecodeAndSaveSeaPenImage(
      const AccountId& account_id,
      const SeaPenImage& sea_pen_image,
      const personalization_app::mojom::SeaPenQueryPtr& query,
      DecodeAndSaveSeaPenImageCallback callback);

  using DeleteRecentSeaPenImageCallback =
      base::OnceCallback<void(bool success)>;

  // Delete the SeaPen image with id `image_id`. Calls `callback` with
  // success=true if the image did exist and was deleted.
  void DeleteSeaPenImage(const AccountId& account_id,
                         uint32_t image_id,
                         DeleteRecentSeaPenImageCallback callback);

  using GetImageIdsCallback =
      base::OnceCallback<void(const std::vector<uint32_t>& ids)>;

  // GetImageIds calls `callback` with a vector of available saved on disk
  // SeaPen image ids for `account_id`.
  void GetImageIds(const AccountId& account_id, GetImageIdsCallback callback);

  using GetImageAndMetadataCallback = base::OnceCallback<void(
      const gfx::ImageSkia& image,
      personalization_app::mojom::RecentSeaPenImageInfoPtr image_info)>;

  // GetImageWithInfo retrieves a full size version of the image saved to disk
  // at `id`. Called with empty `ImageSkia` in case `id` does not exist, or
  // errors reading the file or decoding the data. Also retrieves metadata about
  // the query used to create the image. Does not attempt to retrieve metadata
  // if retrieving the image itself fails.
  void GetImageAndMetadata(const AccountId& account_id,
                           uint32_t image_id,
                           GetImageAndMetadataCallback callback);

  using GetImageCallback =
      base::OnceCallback<void(const gfx::ImageSkia& image)>;

  // GetImage calls GetImageAndMetadata but drops the metadata.
  void GetImage(const AccountId& account_id,
                uint32_t image_id,
                GetImageCallback callback);

 private:
  void SaveSeaPenImage(const AccountId& account_id,
                       uint32_t image_id,
                       const personalization_app::mojom::SeaPenQueryPtr& query,
                       DecodeAndSaveSeaPenImageCallback callback,
                       const gfx::ImageSkia& image_skia);

  void OnSeaPenImageSaved(const gfx::ImageSkia& image_skia,
                          DecodeAndSaveSeaPenImageCallback callback,
                          const base::FilePath& file_path);

  void OnFileRead(GetImageAndMetadataCallback callback, std::string data);

  void OnDecodeImageData(GetImageAndMetadataCallback callback,
                         std::string data,
                         const gfx::ImageSkia& image);

  // The directory where SeaPen images are stored. Initialized as empty
  // FilePath. It is an error to call any method before this directory has been
  // initialized by `SetStorageDirectory`.
  base::FilePath storage_directory_;

  // Not owned. Utility class for saving and loading wallpaper image files.
  raw_ptr<WallpaperFileManager> wallpaper_file_manager_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtrFactory<SeaPenWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
