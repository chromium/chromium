// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator_metrics.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/chrome_constants.h"

namespace ash::browser_data_back_migrator_metrics {

void RecordFinalStatus(BrowserDataBackMigrator::TaskResult result) {
  base::UmaHistogramEnumeration(
      browser_data_back_migrator_metrics::kFinalStatusUMA, result.status);
}

void RecordPosixErrnoIfAvailable(BrowserDataBackMigrator::TaskResult result) {
  if (result.status == BrowserDataBackMigrator::TaskStatus::kSucceeded ||
      !result.posix_errno.has_value()) {
    return;
  }

  const int posix_errno = result.posix_errno.value();
  if (posix_errno == 0) {
    return;
  }

  std::string uma_name = browser_data_back_migrator_metrics::kPosixErrnoUMA +
                         TaskStatusToString(result.status);
  base::UmaHistogramSparse(uma_name, posix_errno);
}

void RecordMigrationTimeIfSuccessful(BrowserDataBackMigrator::TaskResult result,
                                     base::TimeTicks migration_start_time) {
  if (result.status != BrowserDataBackMigrator::TaskStatus::kSucceeded) {
    return;
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kSuccessfulMigrationTimeUMA,
      base::TimeTicks::Now() - migration_start_time);
}

void RecordNumberOfLacrosSecondaryProfiles(
    const base::FilePath& ash_profile_dir) {
  // Since backward migration runs in Ash, calling `GetNumberOfProfiles()` on
  // `ProfileManager` returns the number of profiles in Ash. In order to get the
  // number of secondary profiles in Lacros, the number of directories in the
  // format `chrome::kMultiProfileDirPrefix` + number inside the Lacros
  // directory is counted.

  const base::FilePath lacros_dir =
      ash_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  size_t number_of_secondary_profiles = 0;

  base::FileEnumerator enumerator(lacros_dir, false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    if (!S_ISDIR(info.stat().st_mode)) {
      continue;
    }

    if (IsSecondaryProfileDirectory(entry.BaseName().value())) {
      number_of_secondary_profiles += 1;
    }
  }

  base::UmaHistogramCounts100(
      browser_data_back_migrator_metrics::kNumberOfLacrosSecondaryProfilesUMA,
      number_of_secondary_profiles);
}

void RecordBackwardMigrationTimeDelta(
    absl::optional<base::Time> forward_migration_completion_time) {
  if (!forward_migration_completion_time.has_value()) {
    VLOG(1) << "Forward migration completion time not found.";
    return;
  }

  base::TimeDelta time_delta =
      base::Time::Now() - forward_migration_completion_time.value();

  base::UmaHistogramCustomTimes(
      browser_data_back_migrator_metrics::kElapsedTimeBetweenDataMigrations,
      time_delta, base::Minutes(1), base::Days(24), 100);
}

std::string TaskStatusToString(
    BrowserDataBackMigrator::TaskStatus task_status) {
  switch (task_status) {
#define MAPPING(name)                                \
  case BrowserDataBackMigrator::TaskStatus::k##name: \
    return #name
    MAPPING(Succeeded);
    MAPPING(PreMigrationCleanUpDeleteTmpDirFailed);
    MAPPING(MergeSplitItemsCreateTmpDirFailed);
    MAPPING(MergeSplitItemsCopyExtensionsFailed);
    MAPPING(MergeSplitItemsCopyExtensionStorageFailed);
    MAPPING(MergeSplitItemsCreateDirFailed);
    MAPPING(MergeSplitItemsMergeIndexedDBFailed);
    MAPPING(MergeSplitItemsMergePrefsFailed);
    MAPPING(MergeSplitItemsMergeLocalStorageLevelDBFailed);
    MAPPING(MergeSplitItemsMergeStateStoreLevelDBFailed);
    MAPPING(MergeSplitItemsMergeSyncDataFailed);
    MAPPING(DeleteAshItemsDeleteExtensionsFailed);
    MAPPING(DeleteAshItemsDeleteLacrosItemFailed);
    MAPPING(DeleteLacrosDirDeleteFailed);
    MAPPING(DeleteTmpDirDeleteFailed);
    MAPPING(MoveLacrosItemsToAshDirFailed);
    MAPPING(MoveMergedItemsBackToAshCopyDirectoryFailed);
    MAPPING(MoveMergedItemsBackToAshMoveFileFailed);
#undef MAPPING
  }
}

bool IsSecondaryProfileDirectory(const std::string& dir_base_name) {
  const base::StringPiece prefix = chrome::kMultiProfileDirPrefix;
  const size_t prefix_length = prefix.length();

  if (dir_base_name.length() <= prefix_length) {
    return false;
  }

  bool starts_with_prefix =
      base::StartsWith(dir_base_name, prefix, base::CompareCase::SENSITIVE);
  int number;
  bool ends_with_number =
      base::StringToInt(dir_base_name.substr(prefix_length), &number);

  return starts_with_prefix && ends_with_number;
}

}  // namespace ash::browser_data_back_migrator_metrics
