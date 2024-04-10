// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "ash/webui/common/mojom/sea_pen.mojom-forward.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "ui/gfx/image/image_skia.h"

class AccountId;
class PrefRegistrySimple;
class PrefService;

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace ash {

// A utility class to save / load / delete / enumerate SeaPen images on disk.
// Accessible via a singleton getter.
class ASH_EXPORT SeaPenWallpaperManager {
 public:
  // The result of migrating SeaPen images from global wallpaper directory to
  // user cryptohome. This enum is used for metrics, so enum values should not
  // be changed or reordered. This should be kept in sync with
  // //tools/metrics/histograms/metadata/ash/enums.xml entry
  // `SeaPenWallpaperManagerMigrationStatus`.
  enum class MigrationStatus {
    kNotStarted = 0,
    kCrashed = 1,
    kFailed = 2,
    kSuccess = 3,
    kMaxValue = kSuccess,
  };

  // A useful indirection layer for testing. Allows supplying testing pref
  // services and storage directories.
  class SessionDelegate {
   public:
    virtual ~SessionDelegate() = default;

    virtual base::FilePath GetStorageDirectory(const AccountId& account_id) = 0;

    virtual PrefService* GetPrefService(const AccountId& account_id) = 0;
  };

  SeaPenWallpaperManager();

  SeaPenWallpaperManager(const SeaPenWallpaperManager&) = delete;
  SeaPenWallpaperManager& operator=(const SeaPenWallpaperManager&) = delete;

  ~SeaPenWallpaperManager();

  // `SeaPenWallpaperManager` is owned by and has the same lifetime as
  // `WallpaperController`, so it should exist very early after `Shell` init and
  // last until `Shell` teardown.
  static SeaPenWallpaperManager* GetInstance();

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry_simple);

  using SaveSeaPenImageCallback = base::OnceCallback<void(bool success)>;

  // Decodes Sea Pen image then save the decoded image into disk. Calls
  // `callback` with the boolean success. Note that SeaPen only stores a limited
  // amount of files on disk, so saving additional images may delete the oldest
  // one to make room.
  void SaveSeaPenImage(const AccountId& account_id,
                       const SeaPenImage& sea_pen_image,
                       const personalization_app::mojom::SeaPenQueryPtr& query,
                       SaveSeaPenImageCallback callback);

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

  // Updates the last modified and accessed time so that this file is put at the
  // back of the automatic deletion queue.
  void TouchFile(const AccountId& account_id, uint32_t image_id);

  using GetTemplateIdFromFileCallback =
      base::OnceCallback<void(std::optional<int> template_id)>;

  // Retrieves the template id from the Sea Pen image saved on disk at
  // `image_id`. Calls callback with nullopt if the `image_id` does
  // not exist, or errors reading the file or decoding the data.
  void GetTemplateIdFromFile(const AccountId& account_id,
                             const uint32_t image_id,
                             GetTemplateIdFromFileCallback callback);

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

  bool ShouldMigrate(const AccountId& account_id);

  using MigrateSeaPenFilesIfNecessaryCallback =
      base::OnceCallback<void(bool success)>;

  void Migrate(const AccountId& account_id,
               const base::FilePath& current_directory,
               MigrateSeaPenFilesIfNecessaryCallback callback);

  void SetSessionDelegateForTesting(
      std::unique_ptr<SessionDelegate> session_delegate);

  SessionDelegate* session_delegate_for_testing() {
    return session_delegate_.get();
  }

 private:
  base::FilePath GetFilePathForImageId(const AccountId& account_id,
                                       uint32_t image_id) const;

  void BeginMigration(base::OnceCallback<bool()> migration_task,
                      MigrateSeaPenFilesIfNecessaryCallback callback);

  void OnMigrationComplete(const AccountId& account_id,
                           MigrateSeaPenFilesIfNecessaryCallback callback,
                           bool success);

  void OnSeaPenImageDecoded(
      const AccountId& account_id,
      uint32_t image_id,
      const personalization_app::mojom::SeaPenQueryPtr& query,
      SaveSeaPenImageCallback callback,
      const gfx::ImageSkia& image_skia);

  void OnFileRead(GetImageAndMetadataCallback callback, std::string data);

  void OnFileReadGetTemplateId(GetTemplateIdFromFileCallback callback,
                               const std::string& data);

  std::unique_ptr<SessionDelegate> session_delegate_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SeaPenWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
