// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_METRICS_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_METRICS_H_

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

namespace ash::browser_data_back_migrator_metrics {

constexpr char kFinalStatusUMA[] = "Ash.BrowserDataBackMigrator.FinalStatus";
constexpr char kPosixErrnoUMA[] = "Ash.BrowserDataBackMigrator.PosixErrno.";
constexpr char kSuccessfulMigrationTimeUMA[] =
    "Ash.BrowserDataBackMigrator.SuccessfulMigrationTime";
constexpr char kNumberOfLacrosSecondaryProfilesUMA[] =
    "Ash.BrowserDataBackMigrator.NumberOfLacrosSecondaryProfiles";
constexpr char kElapsedTimeBetweenDataMigrations[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeBetweenDataMigrations";
constexpr char kIsBackwardMigrationPrecededByForwardMigration[] =
    "Ash.BrowserDataBackMigrator.IsPrecededByForwardMigration";

constexpr char kPreMigrationCleanUpTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimePreMigrationCleanUp";
constexpr char kMergeSplitItemsTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeMergeSplitItems";
constexpr char kDeleteAshItemsTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeDeleteAshItems";
constexpr char kMoveLacrosItemsToAshDirTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeMoveLacrosItemsToAshDir";
constexpr char kMoveMergedItemsBackToAshTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeMoveMergedItemsBackToAsh";
constexpr char kDeleteLacrosDirTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeDeleteLacrosDir";
constexpr char kDeleteTmpDirTimeUMA[] =
    "Ash.BrowserDataBackMigrator.ElapsedTimeDeleteTmpDir";

// Records the final status of the migration in `kFinalStatusUMA`.
void RecordFinalStatus(BrowserDataBackMigrator::TaskResult result);

// Records `kPosixErrnoUMA`.`result.status` UMA with the value of
// `result.posix_errno` if the migration failed.
void RecordPosixErrnoIfAvailable(BrowserDataBackMigrator::TaskResult result);

// Records `kSuccessfulMigrationTimeUMA` UMA with the elapsed time since
// starting backward migration. Only recorded if migration was successful.
void RecordMigrationTimeIfSuccessful(BrowserDataBackMigrator::TaskResult result,
                                     base::TimeTicks migration_start_time);

// Records `kNumberOfLacrosSecondaryProfilesUMA` with the number of secondary
// profiles in Lacros at the time of starting backward migration.
void RecordNumberOfLacrosSecondaryProfiles(
    const base::FilePath& ash_profile_dir);

// Records `kElapsedTimeBetweenDataMigrations` with the amount of time between
// successfully completing forward migration and starting backward migration.
void RecordBackwardMigrationTimeDelta(
    absl::optional<base::Time> forward_migration_completion_time);

// Records `kIsBackwardMigrationPrecededByForwardMigration` based on whether
// `forward_migration_completion_time` has a value. This time is only recorded
// if forward migration is actually completed, as opposed to it being marked as
// completed without doing anything.
void RecordBackwardMigrationPrecededByForwardMigration(
    absl::optional<base::Time> forward_migration_completion_time);

// Converts `TaskStatus` to string.
std::string TaskStatusToString(BrowserDataBackMigrator::TaskStatus task_status);

// Checks whether the directory name is in the format expected for a secondary
// profile directory.
bool IsSecondaryProfileDirectory(const std::string& dir_base_name);

}  // namespace ash::browser_data_back_migrator_metrics

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_DATA_BACK_MIGRATOR_METRICS_H_
