// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/copy_migrator.h"

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"
#include "chrome/common/chrome_constants.h"

namespace ash {

CopyMigrator::CopyMigrator(
    const base::FilePath& original_profile_dir,
    const std::string& user_id_hash,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag,
    MigrationFinishedCallback finished_callback)
    : original_profile_dir_(original_profile_dir),
      user_id_hash_(user_id_hash),
      progress_tracker_(std::move(progress_tracker)),
      cancel_flag_(cancel_flag),
      finished_callback_(std::move(finished_callback)) {}

CopyMigrator::~CopyMigrator() = default;

void CopyMigrator::Migrate() {
  // Post `MigrateInternal()` to a worker thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&CopyMigrator::MigrateInternal, original_profile_dir_,
                     std::move(progress_tracker_), cancel_flag_),
      std::move(finished_callback_));
}

// static
BrowserDataMigratorImpl::MigrationResult CopyMigrator::MigrateInternal(
    const base::FilePath& original_profile_dir,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag) {
  BrowserDataMigratorImpl::DataWipeResult data_wipe_result =
      BrowserDataMigratorImpl::DataWipeResult::kSkipped;

  const base::FilePath tmp_dir =
      original_profile_dir.Append(browser_data_migrator_util::kTmpDir);
  const base::FilePath new_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  if (base::DirectoryExists(new_user_dir)) {
    if (!base::DeletePathRecursively(new_user_dir)) {
      PLOG(ERROR) << "Deleting " << new_user_dir.value() << " failed: ";
      UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kDataWipeFailed);
      return {BrowserDataMigratorImpl::DataWipeResult::kFailed,
              {BrowserDataMigrator::ResultKind::kFailed}};
    }
    data_wipe_result = BrowserDataMigratorImpl::DataWipeResult::kSucceeded;
  }

  // Check if tmp directory already exists and delete if it does.
  if (base::PathExists(tmp_dir)) {
    LOG(WARNING) << browser_data_migrator_util::kTmpDir
                 << " already exists indicating migration was aborted on the"
                    "previous attempt.";
    if (!base::DeletePathRecursively(tmp_dir)) {
      PLOG(ERROR) << "Failed to delete tmp dir";
      UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kDeleteTmpDirFailed);
      return {data_wipe_result, {BrowserDataMigrator::ResultKind::kFailed}};
    }
  }

  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir,
          browser_data_migrator_util::ItemType::kNeedCopyForCopy);
  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir, browser_data_migrator_util::ItemType::kLacros);
  const int64_t total_copy_size =
      need_copy_items.total_size + lacros_items.total_size;
  progress_tracker->SetTotalSizeToCopy(total_copy_size);

  base::ElapsedTimer timer;

  uint64_t required_size =
      browser_data_migrator_util::ExtraBytesRequiredToBeFreed(
          total_copy_size, original_profile_dir);
  if (required_size > 0) {
    UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kNotEnoughSpace);
    return {data_wipe_result,
            {BrowserDataMigrator::ResultKind::kFailed, required_size}};
  }

  // Copy files to `tmp_dir`.
  if (!SetupTmpDir(lacros_items, need_copy_items, tmp_dir, cancel_flag.get(),
                   progress_tracker.get())) {
    if (base::PathExists(tmp_dir)) {
      base::DeletePathRecursively(tmp_dir);
    }
    if (cancel_flag->IsSet()) {
      LOG(WARNING) << "Migration was cancelled.";
      UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kCancelled);
      return {data_wipe_result, {BrowserDataMigrator::ResultKind::kCancelled}};
    }

    UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kCopyFailed);
    return {data_wipe_result, {BrowserDataMigrator::ResultKind::kFailed}};
  }

  // Move `tmp_dir` to `new_user_dir`.
  if (!base::Move(tmp_dir, new_user_dir)) {
    PLOG(ERROR) << "Move failed";
    if (base::PathExists(tmp_dir)) {
      base::DeletePathRecursively(tmp_dir);
    }
    UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kMoveFailed);
    return {data_wipe_result, {BrowserDataMigrator::ResultKind::kFailed}};
  }

  LOG(WARNING) << "BrowserDataMigratorImpl::Migrate took "
               << timer.Elapsed().InMilliseconds() << " ms and migrated "
               << total_copy_size / (1024 * 1024) << " MB.";
  UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kSuccess);
  // Record elapsed time for a successful migration.
  // Record byte size. Range 0 ~ 10GB in MBs.
  UMA_HISTOGRAM_CUSTOM_COUNTS(kCopiedDataSize, total_copy_size / 1024 / 1024, 1,
                              10000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      kLacrosDataSize, lacros_items.total_size / 1024 / 1024, 1, 10000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      kCommonDataSize, need_copy_items.total_size / 1024 / 1024, 1, 10000, 100);
  UMA_HISTOGRAM_MEDIUM_TIMES(kTotalTime, timer.Elapsed());
  return {data_wipe_result, {BrowserDataMigratorImpl::ResultKind::kSucceeded}};
}

// static
bool CopyMigrator::SetupTmpDir(
    const browser_data_migrator_util::TargetItems& lacros_items,
    const browser_data_migrator_util::TargetItems& need_copy_items,
    const base::FilePath& tmp_dir,
    browser_data_migrator_util::CancelFlag* cancel_flag,
    MigrationProgressTracker* progress_tracker) {
  if (cancel_flag->IsSet())
    return false;

  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(
          tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath),
          &error)) {
    PLOG(ERROR) << "CreateDirectoryFailed " << error;
    // Maps to histogram enum `PlatformFileError`.
    UMA_HISTOGRAM_ENUMERATION(kCreateDirectoryFail, -error,
                              -base::File::FILE_ERROR_MAX);
    return false;
  }

  // Copy lacros items.
  base::ElapsedTimer timer_for_lacros_items;
  if (!browser_data_migrator_util::CopyTargetItems(
          tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath),
          lacros_items, cancel_flag, progress_tracker)) {
    return false;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES(kLacrosDataTime, timer_for_lacros_items.Elapsed());

  // Copy common items.
  base::ElapsedTimer timer_for_need_copy_items;
  if (!browser_data_migrator_util::CopyTargetItems(
          tmp_dir.Append(browser_data_migrator_util::kLacrosProfilePath),
          need_copy_items, cancel_flag, progress_tracker)) {
    return false;
  }
  UMA_HISTOGRAM_MEDIUM_TIMES(kCommonDataTime,
                             timer_for_need_copy_items.Elapsed());

  // Create `First Run` sentinel file instead of copying. This avoids copying
  // from outside of cryptohome.
  if (!base::WriteFile(tmp_dir.Append(chrome::kFirstRunSentinel), ""))
    return false;

  return true;
}

}  // namespace ash
