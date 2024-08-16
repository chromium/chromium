// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_INFO_MIGRATOR_H_
#define ASH_WALLPAPER_WALLPAPER_INFO_MIGRATOR_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace ash {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MigrationStatus {
  // The migration succeeded.
  kSucceeded = 0,
  // The migration failed.
  kFailed,
  // The migration is not supported due to no location.
  kNotSupportedNoLocation,
  // The migration is not supported due to no collection.
  kNotSupportedNoCollection,

  kMaxValue = kNotSupportedNoCollection,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class MigrationFailureReason {
  kOnlineVariantsFetchFailure = 0,
  kOnlineNoValidVariants = 1,

  // Add new values above this line.
  kMaxValue = kOnlineNoValidVariants,
};

// Facilitates the migration of WallpaperInfo objects from non or older versions
// to the latest supported version. For more info, see
// go/cros-versioned-wallpaper-info
class WallpaperInfoMigrator {
 public:
  // Callback that is run at the end of the migration. WallpaperInfo is only
  // present if the migration succeeded.
  using MigrateCallback =
      base::OnceCallback<void(const std::optional<WallpaperInfo>&)>;

  WallpaperInfoMigrator();
  WallpaperInfoMigrator(const WallpaperInfoMigrator&) = delete;
  WallpaperInfoMigrator& operator=(const WallpaperInfoMigrator&) = delete;
  ~WallpaperInfoMigrator();

  // Returns true if the given `info` should be migrated.
  static bool ShouldMigrate(const WallpaperInfo& info);

  // Carries out the migration. `callback` will be called at the end of the
  // migration.
  void Migrate(const AccountId& account_id,
               const WallpaperInfo& unmigrated_info,
               MigrateCallback callback);

 private:
  // Fills `unit_id` and `variants` for online wallpapers.
  void PopulateMissingOnlineWallpaperData(
      base::OnceCallback<void(std::optional<WallpaperInfo>)> callback,
      const WallpaperInfo& unmigrated_info,
      std::optional<OnlineWallpaperParams> params);

  // Runs as the final step of the migration.
  void OnMigrationCompleted(WallpaperType type,
                            std::optional<WallpaperInfo> migrated_info);

  // Used for metrics. Indicates the start time of the migration.
  base::Time migrate_start_time_;

  // Callback to be called once migration is done. It is called regardless of
  // whether migration succeeded or not.
  MigrateCallback completion_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WallpaperInfoMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_INFO_MIGRATOR_H_
