// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_info_migrator.h"

#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/online_wallpaper_variant_info_fetcher.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace ash {

namespace {

void RecordMigrationStatus(WallpaperType type, MigrationStatus status) {
  base::UmaHistogramEnumeration(
      base::StringPrintf("Ash.Wallpaper.%s.MigrationStatus",
                         WallpaperTypeToString(type).c_str()),
      status);
}

void RecordMigrationFailureReason(MigrationFailureReason reason) {
  base::UmaHistogramEnumeration("Ash.Wallpaper.MigrationFailureReason", reason);
}

void RecordMigrationLatency(WallpaperType type, base::TimeDelta latency) {
  base::UmaHistogramTimes(
      base::StringPrintf("Ash.Wallpaper.%s.MigrationLatency",
                         WallpaperTypeToString(type).c_str()),
      latency);
}

// Checks whether migration is supported for the online wallpaper info. If not,
// logs the reason.
bool IsMigrationSupportedForOnlineWallpaper(
    const WallpaperInfo& unmigrated_info) {
  CHECK(IsOnlineWallpaper(unmigrated_info.type));
  if (unmigrated_info.location.empty()) {
    LOG(WARNING) << __func__ << " Unable to migrate due to empty location";
    RecordMigrationStatus(unmigrated_info.type,
                          MigrationStatus::kNotSupportedNoLocation);
    return false;
  }
  if (unmigrated_info.collection_id.empty()) {
    LOG(WARNING) << __func__ << " Unable to migrate due to empty collection";
    RecordMigrationStatus(unmigrated_info.type,
                          MigrationStatus::kNotSupportedNoCollection);
    return false;
  }

  return true;
}

}  // namespace

WallpaperInfoMigrator::WallpaperInfoMigrator() = default;

WallpaperInfoMigrator::~WallpaperInfoMigrator() = default;

// static
bool WallpaperInfoMigrator::ShouldMigrate(const WallpaperInfo& info) {
  if (!features::IsVersionWallpaperInfoEnabled()) {
    return false;
  }

  // Skips migration if the wallpaper is already migrated or not supported by
  // the current version.
  return !info.version.IsValid() ||
         info.version < GetSupportedVersion(info.type);
}

void WallpaperInfoMigrator::Migrate(const AccountId& account_id,
                                    const WallpaperInfo& unmigrated_info,
                                    MigrateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  if (completion_callback_) {
    LOG(WARNING) << __func__ << " Unexpected on-going migration. Cancelling.";
    std::move(completion_callback_).Run(std::nullopt);
  }
  completion_callback_ = std::move(callback);
  migrate_start_time_ = base::Time::Now();
  DVLOG(0) << __func__ << " Applying migration for info=" << unmigrated_info;
  if (IsOnlineWallpaper(unmigrated_info.type)) {
    if (!IsMigrationSupportedForOnlineWallpaper(unmigrated_info)) {
      RecordMigrationLatency(unmigrated_info.type,
                             base::Time::Now() - migrate_start_time_);
      std::move(completion_callback_).Run(std::nullopt);
      return;
    }

    auto on_complete =
        base::BindOnce(&WallpaperInfoMigrator::OnMigrationCompleted,
                       weak_factory_.GetWeakPtr(), unmigrated_info.type);
    bool should_fetch_variants =
        !unmigrated_info.unit_id || unmigrated_info.variants.empty();
    if (!should_fetch_variants) {
      std::move(on_complete).Run(unmigrated_info);
      return;
    }

    auto* variant_fetcher = OnlineWallpaperVariantInfoFetcher::GetInstance();
    OnlineWallpaperVariantInfoFetcher::FetchParamsCallback on_fetch_done =
        base::BindOnce(
            &WallpaperInfoMigrator::PopulateMissingOnlineWallpaperData,
            weak_factory_.GetWeakPtr(), std::move(on_complete),
            unmigrated_info);
    variant_fetcher->FetchOnlineWallpaper(account_id, unmigrated_info,
                                          std::move(on_fetch_done));
    return;
  }

  // Handle other wallpaper types.
  auto on_complete =
      base::BindOnce(&WallpaperInfoMigrator::OnMigrationCompleted,
                     weak_factory_.GetWeakPtr(), unmigrated_info.type);
  std::move(on_complete).Run(unmigrated_info);
}

void WallpaperInfoMigrator::PopulateMissingOnlineWallpaperData(
    base::OnceCallback<void(std::optional<WallpaperInfo>)> callback,
    const WallpaperInfo& unmigrated_info,
    std::optional<OnlineWallpaperParams> params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!params) {
    LOG(ERROR) << __func__ << " Failed to fetch online wallpaper for migration";

    RecordMigrationFailureReason(
        MigrationFailureReason::kOnlineVariantsFetchFailure);
    std::move(callback).Run(std::nullopt);
    return;
  }

  // It's safe to retrieve checkpoint from `DarkLightModeControllerImpl` instead
  // of `WallpaperTimeOfDayScheduler` because time-of-day wallpapers do not need
  // to be migrated.
  const ScheduleCheckpoint checkpoint =
      Shell::Get()->dark_light_mode_controller()->current_checkpoint();
  const OnlineWallpaperVariant* selected_variant =
      FirstValidVariant(params->variants, checkpoint);
  if (!selected_variant) {
    LOG(ERROR) << "Failed to select online wallpaper variant for migration";
    RecordMigrationFailureReason(
        MigrationFailureReason::kOnlineNoValidVariants);
    std::move(callback).Run(std::nullopt);
    return;
  }
  WallpaperInfo migrated_info = WallpaperInfo(*params, *selected_variant);
  // Migrated info should not have its date modified.
  migrated_info.date = unmigrated_info.date;

  std::move(callback).Run(migrated_info);
}

void WallpaperInfoMigrator::OnMigrationCompleted(
    WallpaperType type,
    std::optional<WallpaperInfo> migrated_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(completion_callback_);
  MigrationStatus status = MigrationStatus::kFailed;
  if (migrated_info) {
    migrated_info->version = GetSupportedVersion(migrated_info->type);
    status = MigrationStatus::kSucceeded;
  }
  RecordMigrationStatus(type, status);
  RecordMigrationLatency(type, base::Time::Now() - migrate_start_time_);
  std::move(completion_callback_).Run(migrated_info);
}

}  // namespace ash
