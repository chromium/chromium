// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

#include <errno.h>

#include <memory>
#include <string>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator_metrics.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash {

namespace {

// Flag values for `switches::kForceBrowserDataBackwardMigrationForTesting`.
const char kBrowserDataBackwardMigrationForceSkip[] = "force-skip";
const char kBrowserDataBackwardMigrationForceMigration[] = "force-migration";

base::RepeatingClosure* g_back_migrator_attempt_restart_for_testing = nullptr;

// We set a generous recursion depth, that should never be reached, but this
// way we protect against file system loops.
const unsigned int kMaxRecursionDepth = 2000;
}  // namespace

ScopedBackMigratorRestartAttemptForTesting::
    ScopedBackMigratorRestartAttemptForTesting(
        base::RepeatingClosure callback) {
  DCHECK(!g_back_migrator_attempt_restart_for_testing);
  g_back_migrator_attempt_restart_for_testing =
      new base::RepeatingClosure(std::move(callback));
}

ScopedBackMigratorRestartAttemptForTesting::
    ~ScopedBackMigratorRestartAttemptForTesting() {
  DCHECK(g_back_migrator_attempt_restart_for_testing);
  delete g_back_migrator_attempt_restart_for_testing;
  g_back_migrator_attempt_restart_for_testing = nullptr;
}

BrowserDataBackMigrator::BrowserDataBackMigrator(
    const base::FilePath& ash_profile_dir,
    const std::string& user_id_hash,
    PrefService* local_state)
    : ash_profile_dir_(ash_profile_dir),
      user_id_hash_(user_id_hash),
      local_state_(local_state) {}

BrowserDataBackMigrator::~BrowserDataBackMigrator() = default;

// static
void BrowserDataBackMigrator::AttemptRestart() {
  if (g_back_migrator_attempt_restart_for_testing) {
    g_back_migrator_attempt_restart_for_testing->Run();
    return;
  }

  chrome::AttemptRestart();
}

void BrowserDataBackMigrator::Migrate(
    BackMigrationProgressCallback progress_callback,
    BackMigrationFinishedCallback finished_callback) {
  LOG(WARNING) << "BrowserDataBackMigrator::Migrate() is called.";

  DCHECK(!running_);
  DCHECK(IsBackMigrationEnabled(
      ash::standalone_browser::migrator_util::PolicyInitState::kBeforeInit));

  browser_data_back_migrator_metrics::RecordNumberOfLacrosSecondaryProfiles(
      ash_profile_dir_);

  // Get the forward migration timestamp, record the delta and then clear the
  // timestamp right away. This prevents the scenario in which the time is
  // recorded, then backward migration fails and is retried and then the time is
  // recorded again.
  auto forward_migration_completion_time =
      crosapi::browser_util::GetProfileMigrationCompletionTimeForUser(
          local_state_, user_id_hash_);
  browser_data_back_migrator_metrics::RecordBackwardMigrationTimeDelta(
      forward_migration_completion_time);
  browser_data_back_migrator_metrics::
      RecordBackwardMigrationPrecededByForwardMigration(
          forward_migration_completion_time);
  crosapi::browser_util::ClearProfileMigrationCompletionTimeForUser(
      local_state_, user_id_hash_);

  running_ = true;
  migration_start_time_ = base::TimeTicks::Now();

  const base::FilePath lacros_dir =
      ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);

  progress_callback_ = std::move(progress_callback);
  finished_callback_ = std::move(finished_callback);

  SetProgress(MigrationStep::kPreMigrationCleanUp);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::PreMigrationCleanUp,
                     ash_profile_dir_, lacros_dir),
      base::BindOnce(&BrowserDataBackMigrator::OnPreMigrationCleanUp,
                     weak_factory_.GetWeakPtr()));
}

void BrowserDataBackMigrator::SetProgress(MigrationStep step) {
  int current_step = static_cast<int>(step);
  int total_steps = static_cast<int>(MigrationStep::kMaxValue);
  int percent = (current_step * 100) / total_steps;

  progress_callback_.Run(percent);
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::PreMigrationCleanUp(
    const base::FilePath& ash_profile_dir,
    const base::FilePath& lacros_dir) {
  LOG(WARNING) << "Running PreMigrationCleanUp()";
  base::ElapsedTimer timer;

  const base::FilePath tmp_profile_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);
  if (base::PathExists(tmp_profile_dir)) {
    // Delete tmp_profile_dir if any were left from a previous failed back
    // migration attempt.
    if (!base::DeletePathRecursively(tmp_profile_dir)) {
      PLOG(ERROR) << "Deleting " << tmp_profile_dir.value() << " failed: ";
      return {TaskStatus::kPreMigrationCleanUpDeleteTmpDirFailed, errno};
    }
  }

  // Delete ash deletable items to free up space.
  browser_data_migrator_util::TargetItems ash_deletable_items =
      browser_data_migrator_util::GetTargetItems(
          ash_profile_dir, browser_data_migrator_util::ItemType::kDeletable);
  for (const auto& item : ash_deletable_items.items) {
    bool result = item.is_directory ? base::DeletePathRecursively(item.path)
                                    : base::DeleteFile(item.path);
    if (!result) {
      // This is not critical to the migration so log the error, but do not stop
      // the migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  // Delete lacros deletable items to free up space.
  browser_data_migrator_util::TargetItems lacros_deletable_items =
      browser_data_migrator_util::GetTargetItems(
          lacros_dir, browser_data_migrator_util::ItemType::kDeletable);
  for (const auto& item : lacros_deletable_items.items) {
    bool result = item.is_directory ? base::DeletePathRecursively(item.path)
                                    : base::DeleteFile(item.path);
    if (!result) {
      // This is not critical to the migration so log the error, but do not stop
      // the migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kPreMigrationCleanUpTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnPreMigrationCleanUp(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "PreMigrationCleanup() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kMergeSplitItems);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::MergeSplitItems,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnMergeSplitItems,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::MergeSplitItems(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running MergeSplitItems()";
  base::ElapsedTimer timer;

  const base::FilePath tmp_profile_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);

  if (!base::CreateDirectory(tmp_profile_dir)) {
    PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_profile_dir.value();
    return {TaskStatus::kMergeSplitItemsCreateTmpDirFailed, errno};
  }

  const base::FilePath lacros_default_profile_dir =
      ash_profile_dir.Append(browser_data_migrator_util::kLacrosDir)
          .Append(browser_data_migrator_util::kLacrosProfilePath);

  // For extensions that exist in both Ash and Lacros, take the Lacros version.
  if (!MergeCommonExtensionsDataFiles(
          ash_profile_dir, lacros_default_profile_dir, tmp_profile_dir,
          browser_data_migrator_util::kExtensionsFilePath)) {
    PLOG(ERROR) << "MergeCommonExtensionsDataFiles() failed for extensions";
    return {TaskStatus::kMergeSplitItemsCopyExtensionsFailed, errno};
  }

  // For Storage objects for extensions that exist in both Ash and Lacros, take
  // the Lacros version.
  if (!MergeCommonExtensionsDataFiles(
          ash_profile_dir, lacros_default_profile_dir, tmp_profile_dir,
          base::FilePath(browser_data_migrator_util::kStorageFilePath)
              .Append(browser_data_migrator_util::kStorageExtFilePath)
              .value())) {
    PLOG(ERROR)
        << "MergeCommonExtensionsDataFiles() failed for extension storage";
    return {TaskStatus::kMergeSplitItemsCopyExtensionStorageFailed, errno};
  }

  // Merge IndexedDB.
  for (const auto& extension_id :
       extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser()) {
    if (!MergeCommonIndexedDB(ash_profile_dir, lacros_default_profile_dir,
                              extension_id.data())) {
      return {TaskStatus::kMergeSplitItemsMergeIndexedDBFailed, errno};
    }
  }

  // During forward migration, LevelDB databases were copied from Ash to Lacros
  // verbatim, after which the Ash versions were edited and non-Ash entries were
  // removed. As a result Lacros has entries that it does not need and never
  // updates, which don't break anything and don't take up a lot of space.
  // During backward migration, we take the Lacros version as basis and then
  // overwrite some entries with the Ash entries that might have been changed.

  // Merge `Local Storage` LevelDB database.
  if (!CopyLevelDBBase(
          lacros_default_profile_dir.Append(
              browser_data_migrator_util::kLocalStorageFilePath),
          tmp_profile_dir.Append(
              browser_data_migrator_util::kLocalStorageFilePath))) {
    LOG(ERROR) << "CopyLevelDBBase() failed for `Local Storage`";
    return {TaskStatus::kMergeSplitItemsMergeLocalStorageLevelDBFailed};
  }
  if (!MergeLevelDB(
          ash_profile_dir
              .Append(browser_data_migrator_util::kLocalStorageFilePath)
              .Append(browser_data_migrator_util::kLocalStorageLeveldbName),
          tmp_profile_dir
              .Append(browser_data_migrator_util::kLocalStorageFilePath)
              .Append(browser_data_migrator_util::kLocalStorageLeveldbName),
          browser_data_migrator_util::LevelDBType::kLocalStorage)) {
    LOG(ERROR) << "MergeLevelDB() failed for `Local Storage`";
    return {TaskStatus::kMergeSplitItemsMergeLocalStorageLevelDBFailed};
  }

  // Merge `kStateStorePaths` LevelDB databases.
  for (const char* path : browser_data_migrator_util::kStateStorePaths) {
    if (base::PathExists(lacros_default_profile_dir.Append(path))) {
      if (!CopyLevelDBBase(lacros_default_profile_dir.Append(path),
                           tmp_profile_dir.Append(path))) {
        LOG(ERROR) << "CopyLevelDBBase() failed for `" << path << "`";
        return {TaskStatus::kMergeSplitItemsMergeStateStoreLevelDBFailed};
      }
      if (!MergeLevelDB(ash_profile_dir.Append(path),
                        tmp_profile_dir.Append(path),
                        browser_data_migrator_util::LevelDBType::kStateStore)) {
        LOG(ERROR) << "MergeLevelDB() failed for `" << path << "`";
        return {TaskStatus::kMergeSplitItemsMergeStateStoreLevelDBFailed};
      }
    }
  }

  // Merge Preferences.
  const base::FilePath ash_pref_path =
      ash_profile_dir.Append(chrome::kPreferencesFilename);
  const base::FilePath lacros_pref_path =
      lacros_default_profile_dir.Append(chrome::kPreferencesFilename);
  const base::FilePath tmp_pref_path =
      tmp_profile_dir.Append(chrome::kPreferencesFilename);
  if (!MergePreferences(ash_pref_path, lacros_pref_path, tmp_pref_path)) {
    return {TaskStatus::kMergeSplitItemsMergePrefsFailed};
  }

  // Merge Sync Data.
  const base::FilePath ash_sync_data_db_path =
      ash_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
          .Append(browser_data_migrator_util::kSyncDataLeveldbName);
  const base::FilePath lacros_sync_data_db_path =
      lacros_default_profile_dir
          .Append(browser_data_migrator_util::kSyncDataFilePath)
          .Append(browser_data_migrator_util::kSyncDataLeveldbName);
  const base::FilePath tmp_sync_data_db_path =
      tmp_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
          .Append(browser_data_migrator_util::kSyncDataLeveldbName);

  if (!MergeSyncDataLevelDB(ash_sync_data_db_path, lacros_sync_data_db_path,
                            tmp_sync_data_db_path)) {
    return {TaskStatus::kMergeSplitItemsMergeSyncDataFailed};
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kMergeSplitItemsTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnMergeSplitItems(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MergeSplitItems() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kDeleteAshItems);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::DeleteAshItems,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnDeleteAshItems,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::DeleteAshItems(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running DeleteAshItems()";
  base::ElapsedTimer timer;

  // For extensions that exist in both Ash and Lacros, take the Lacros version
  // and delete the Ash version.
  if (!RemoveAshCommonExtensionsDataFiles(
          ash_profile_dir, browser_data_migrator_util::kExtensionsFilePath)) {
    PLOG(ERROR) << "RemoveAshCommonExtensionsDataFiles() failed for extensions";
    return {TaskStatus::kDeleteAshItemsDeleteExtensionsFailed, errno};
  }

  // `ItemType::kLacros` items should be deleted from Ash before they are
  // overwritten by `MoveMergedItemsBackToAsh`. We call
  // `base::DeletePathRecursively` because it deletes both files and directories
  // and it does not fail if an item does not exist.
  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          ash_profile_dir, browser_data_migrator_util::ItemType::kLacros);
  for (const auto& item : lacros_items.items) {
    // Print permissions for debugging purposes to be able to compare with the
    // persmissions of the directories created in `MoveMergedItemsBackToAsh`.
    int permissions;
    if (base::GetPosixFilePermissions(item.path, &permissions)) {
      VLOG(5) << "Deleting " << item.path.value() << " with permissions "
              << permissions;
    }

    if (!base::DeletePathRecursively(item.path)) {
      PLOG(ERROR) << "Failed to delete " << item.path.value();
      return {TaskStatus::kDeleteAshItemsDeleteLacrosItemFailed, errno};
    }
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kDeleteAshItemsTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnDeleteAshItems(TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "DeleteAshItems() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kMoveLacrosItemsToAshDir);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::MoveLacrosItemsToAshDir,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnMoveLacrosItemsToAshDir,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::MoveLacrosItemsToAshDir(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running MoveLacrosItemsToAshDir()";
  base::ElapsedTimer timer;

  const base::FilePath lacros_default_profile_dir =
      ash_profile_dir.Append(browser_data_migrator_util::kLacrosDir)
          .Append(browser_data_migrator_util::kLacrosProfilePath);

  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          lacros_default_profile_dir,
          browser_data_migrator_util::ItemType::kLacros);

  for (const auto& item : lacros_items.items) {
    // The corresponding items in Ash are deleted in `DeleteAshItems` before
    // they are overwritten by the Lacros items here.
    const base::FilePath destination_path =
        ash_profile_dir.Append(item.path.BaseName());

    if (base::PathExists(destination_path)) {
      PLOG(ERROR) << "Path " << destination_path << " already exists.";
      return {TaskStatus::kMoveLacrosItemsToAshDirFailed, errno};
    }

    if (!base::Move(item.path, destination_path)) {
      PLOG(ERROR) << "Failed to move item " << item.path.value() << " to "
                  << destination_path << ": ";
      return {TaskStatus::kMoveLacrosItemsToAshDirFailed, errno};
    }
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kMoveLacrosItemsToAshDirTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnMoveLacrosItemsToAshDir(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MoveLacrosItemsToAshDir() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kMoveMergedItemsBackToAsh);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::MoveMergedItemsBackToAsh,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnMoveMergedItemsBackToAsh,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::MoveMergedItemsBackToAsh(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running MoveMergedItemsBackToAsh()";
  base::ElapsedTimer timer;

  const base::FilePath tmp_profile_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);

  if (!MoveFilesToAshDirectory(tmp_profile_dir, ash_profile_dir, 1)) {
    PLOG(ERROR) << "Failed moving " << tmp_profile_dir.value() << " to "
                << ash_profile_dir.value();
    return {TaskStatus::kMoveMergedItemsBackToAshMoveFileFailed, errno};
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kMoveMergedItemsBackToAshTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

// static
bool BrowserDataBackMigrator::MoveFilesToAshDirectory(
    const base::FilePath& source_dir,
    const base::FilePath& dest_dir,
    unsigned int recursion_depth) {
  VLOG(5) << "Calling MoveFilesToAshDirectory from " << source_dir.value()
          << " to " << dest_dir.value() << " at recursion depth "
          << recursion_depth;

  if (recursion_depth >= kMaxRecursionDepth) {
    LOG(WARNING) << "We have reached maximum recursion depth "
                 << kMaxRecursionDepth
                 << " and we are stopping MoveFilesToAshDirectory()";
    return false;
  }

  base::FileEnumerator enumerator(
      source_dir, false /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    const base::FilePath source_path = source_dir.Append(entry.BaseName());
    if (S_ISREG(info.stat().st_mode)) {
      if (!base::Move(source_path, dest_dir.Append(entry.BaseName()))) {
        PLOG(ERROR) << "Failed moving " << source_path.value() << " to "
                    << dest_dir.value();
        return false;
      }
    } else if (S_ISDIR(info.stat().st_mode)) {
      const base::FilePath& new_source_dir =
          source_dir.Append(entry.BaseName());
      const base::FilePath& new_dest_dir = dest_dir.Append(entry.BaseName());
      if (!base::PathExists(new_dest_dir)) {
        if (!base::CreateDirectory(new_dest_dir)) {
          PLOG(ERROR) << "Failed to create " << new_dest_dir.value();
          return false;
        }

        // Print permissions for debugging purposes to be able to compare with
        // the persmissions of the directories deleted in `DeleteAshItems`.
        int permissions;
        if (base::GetPosixFilePermissions(new_dest_dir, &permissions)) {
          VLOG(5) << "Created " << new_dest_dir << " with permissions "
                  << permissions;
        }
      }

      if (!MoveFilesToAshDirectory(new_source_dir, new_dest_dir,
                                   ++recursion_depth)) {
        return false;
      }
    } else {
      PLOG(ERROR) << "Received an entry that is neither a file nor a directory "
                  << entry;
      return false;
    }
  }

  return true;
}

void BrowserDataBackMigrator::OnMoveMergedItemsBackToAsh(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MoveMergedItemsBackToAsh() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kDeleteLacrosDir);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::DeleteLacrosDir,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnDeleteLacrosDir,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::DeleteLacrosDir(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running DeleteLacrosDir()";
  base::ElapsedTimer timer;

  const base::FilePath lacros_dir =
      ash_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  if (base::PathExists(lacros_dir)) {
    if (!base::DeletePathRecursively(lacros_dir)) {
      PLOG(ERROR) << "Deleting " << lacros_dir.value() << " failed: ";
      return {TaskStatus::kDeleteLacrosDirDeleteFailed, errno};
    }
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kDeleteLacrosDirTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnDeleteLacrosDir(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "DeleteLacrosDir() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kDeleteTmpDir);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::DeleteTmpDir, ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnDeleteTmpDir,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::DeleteTmpDir(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running DeleteTmpDir()";
  base::ElapsedTimer timer;

  const base::FilePath tmp_user_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);
  if (base::PathExists(tmp_user_dir)) {
    if (!base::DeletePathRecursively(tmp_user_dir)) {
      PLOG(ERROR) << "Deleting " << tmp_user_dir.value() << " failed: ";
      return {TaskStatus::kDeleteTmpDirDeleteFailed, errno};
    }
  }

  base::UmaHistogramMediumTimes(
      browser_data_back_migrator_metrics::kDeleteTmpDirTimeUMA,
      timer.Elapsed());

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnDeleteTmpDir(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "DeleteTmpDir() failed.";
    InvokeCallback(result);
    return;
  }

  SetProgress(MigrationStep::kMarkMigrationComplete);
  // MarkMigrationComplete needs to run on the UI thread first to update prefs.
  MarkMigrationComplete();
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::MarkMigrationComplete() {
  LOG(WARNING) << "Running MarkMigrationComplete()";

  crosapi::browser_util::SetProfileDataBackwardMigrationCompletedForUser(
      local_state_, user_id_hash_);

  local_state_->CommitPendingWrite(
      base::BindOnce(&BrowserDataBackMigrator::OnMarkMigrationComplete,
                     weak_factory_.GetWeakPtr()));

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnMarkMigrationComplete() {
  LOG(WARNING) << "Backward migration completed successfully.";
  SetProgress(MigrationStep::kDone);
  InvokeCallback({TaskStatus::kSucceeded});
}

void BrowserDataBackMigrator::CancelMigration(
    BackMigrationCanceledCallback canceled_callback) {
  LOG(WARNING) << "BrowserDataBackMigrator::CancelMigration() is called.";

  canceled_callback_ = std::move(canceled_callback);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::FailedMigrationCleanUp,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnFailedMigrationCleanUp,
                     weak_factory_.GetWeakPtr()));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::FailedMigrationCleanUp(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running FailedMigrationCleanUp()";

  auto delete_lacros_dir_result =
      BrowserDataBackMigrator::DeleteLacrosDir(ash_profile_dir);
  auto delete_tmp_dir_result =
      BrowserDataBackMigrator::DeleteTmpDir(ash_profile_dir);

  if (delete_lacros_dir_result.status != TaskStatus::kSucceeded) {
    return delete_lacros_dir_result;
  }

  if (delete_tmp_dir_result.status != TaskStatus::kSucceeded) {
    return delete_tmp_dir_result;
  }

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnFailedMigrationCleanUp(
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "FailedMigrationCleanUp() failed.";
  } else {
    LOG(WARNING) << "Backward migration cancellation completed successfully";
  }
  // TODO(b/272017148): Add UMA metrics.
  std::move(canceled_callback_).Run();
}

// static
bool BrowserDataBackMigrator::MergeCommonExtensionsDataFiles(
    const base::FilePath& ash_profile_dir,
    const base::FilePath& lacros_default_profile_dir,
    const base::FilePath& tmp_profile_dir,
    const std::string& target_dir) {
  // For objects that are in both Chromes copy the Lacros version to the
  // temporary folder.
  const base::FilePath lacros_target_dir =
      lacros_default_profile_dir.Append(target_dir);

  if (base::PathExists(lacros_target_dir)) {
    const base::FilePath tmp_target_dir = tmp_profile_dir.Append(target_dir);
    if (!base::CreateDirectory(tmp_target_dir)) {
      PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_target_dir.value();
      return false;
    }

    for (const auto& extension_id :
         extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser()) {
      base::FilePath lacros_target_path =
          lacros_target_dir.Append(extension_id);

      if (base::PathExists(lacros_target_path)) {
        base::FilePath tmp_target_path = tmp_target_dir.Append(extension_id);

        if (!base::CopyDirectory(lacros_target_path, tmp_target_path,
                                 /*recursive=*/true)) {
          PLOG(ERROR) << "Failed copying " << lacros_target_path.value()
                      << " to " << tmp_target_path.value();
          return false;
        }
      }
    }
  }

  return true;
}

// static
bool BrowserDataBackMigrator::RemoveAshCommonExtensionsDataFiles(
    const base::FilePath& ash_profile_dir,
    const std::string& target_dir) {
  // For objects that are in both Chromes delete the Ash version for those
  // objects before it is overwritten by MoveMergedItemsBackToAsh.
  const base::FilePath ash_target_dir = ash_profile_dir.Append(target_dir);

  if (base::PathExists(ash_target_dir)) {
    for (const auto& extension_id :
         extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser()) {
      base::FilePath ash_target_path = ash_target_dir.Append(extension_id);

      if (!base::DeletePathRecursively(ash_target_path)) {
        PLOG(ERROR) << "Failed deleting " << ash_target_path.value();
        return false;
      }
    }
  }
  return true;
}

// static
bool BrowserDataBackMigrator::MergeCommonIndexedDB(
    const base::FilePath& ash_profile_dir,
    const base::FilePath& lacros_default_profile_dir,
    const char* extension_id) {
  const auto& [ash_blob_path, ash_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(ash_profile_dir,
                                                    extension_id);

  const auto& [lacros_blob_path, lacros_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(lacros_default_profile_dir,
                                                    extension_id);

  if (base::PathExists(lacros_blob_path)) {
    if (base::PathExists(ash_blob_path)) {
      if (!base::DeletePathRecursively(ash_blob_path)) {
        PLOG(ERROR) << "Failed deleting " << ash_blob_path.value();
        return false;
      }
    }

    if (!base::CreateDirectory(ash_blob_path)) {
      PLOG(ERROR) << "Failed creating empty " << ash_blob_path.value();
      return false;
    }

    if (!base::Move(lacros_blob_path, ash_blob_path)) {
      PLOG(ERROR) << "Failed migrating " << lacros_blob_path.value() << " to "
                  << ash_blob_path.value();
      return false;
    }
  }

  if (base::PathExists(lacros_leveldb_path)) {
    if (base::PathExists(ash_leveldb_path)) {
      if (!base::DeletePathRecursively(ash_leveldb_path)) {
        PLOG(ERROR) << "Failed deleting " << ash_leveldb_path.value();
        return false;
      }
    }

    if (!base::CreateDirectory(ash_leveldb_path)) {
      PLOG(ERROR) << "Failed creating empty " << ash_leveldb_path.value();
      return false;
    }

    if (!base::Move(lacros_leveldb_path, ash_leveldb_path)) {
      PLOG(ERROR) << "Failed migrating " << lacros_leveldb_path.value()
                  << " to " << ash_leveldb_path.value();
      return false;
    }
  }

  return true;
}

// static
bool BrowserDataBackMigrator::MergePreferences(
    const base::FilePath& ash_pref_path,
    const base::FilePath& lacros_pref_path,
    const base::FilePath& tmp_pref_path) {
  // Get string contents of the Ash file.
  std::string ash_contents;
  if (!base::ReadFileToString(ash_pref_path, &ash_contents)) {
    PLOG(ERROR) << "Failure while opening Ash Preferences: "
                << ash_pref_path.value();
    return false;
  }

  // Parse the Ash JSON file.
  std::optional<base::Value> ash_root = base::JSONReader::Read(ash_contents);
  if (!ash_root) {
    PLOG(ERROR) << "Failure while parsing Ash's Preferences";
    return false;
  }
  base::Value::Dict* ash_root_dict = ash_root->GetIfDict();
  if (!ash_root_dict) {
    PLOG(ERROR) << "Failure while parsing Ash's Preferences root node";
    return false;
  }

  // Get string contents of the Lacros file.
  std::string lacros_contents;
  if (!base::ReadFileToString(lacros_pref_path, &lacros_contents)) {
    PLOG(ERROR) << "Failure while opening Lacros Preferences: "
                << lacros_pref_path.value();
    return false;
  }

  // Parse the Lacros JSON file.
  std::optional<base::Value> lacros_root =
      base::JSONReader::Read(lacros_contents);
  if (!lacros_root) {
    PLOG(ERROR) << "Failure while parsing Lacros's Preferences";
    return false;
  }
  base::Value::Dict* lacros_root_dict = lacros_root->GetIfDict();
  if (!lacros_root_dict) {
    PLOG(ERROR) << "Failure while parsing Lacros's Preferences root node";
    return false;
  }

  MergeLacrosPreferences(*ash_root_dict, {}, lacros_root.value());

  // Preferences that were split between Ash and Lacros relate to extensions.
  // Here we need to take the preferences from Lacros that were removed from
  // Ash during forward migration and put them back in Ash.
  for (const char* key : browser_data_migrator_util::kSplitPreferencesKeys) {
    base::Value* ash_value = ash_root_dict->FindByDottedPath(key);
    base::Value* lacros_value = lacros_root_dict->FindByDottedPath(key);

    // If there is nothing to copy back from Lacros, skip this preference.
    if (!lacros_value)
      continue;

    // If there is no Ash counterpart for this preference, clone Lacros.
    if (!ash_value) {
      ash_root_dict->SetByDottedPath(key, lacros_value->Clone());
    } else {
      if (lacros_value->is_dict() && ash_value->is_dict()) {
        for (const auto entry : lacros_value->GetDict()) {
          const std::string_view extension_id = entry.first;
          if (IsLacrosOnlyExtension(extension_id)) {
            ash_value->GetDict().Set(extension_id, entry.second.Clone());
          }
        }
      } else if (lacros_value->is_list() && ash_value->is_list()) {
        ash_value->GetList().EraseIf([&](const base::Value& item) {
          if (!item.is_string())
            return false;

          const std::string_view extension_id = item.GetString();
          return IsLacrosOnlyExtension(extension_id);
        });

        for (const auto& entry : lacros_value->GetList()) {
          if (!entry.is_string())
            continue;

          if (IsLacrosOnlyExtension(entry.GetString()))
            ash_value->GetList().Append(entry.Clone());
        }
      }
    }
  }

  // Generate the resulting JSON.
  std::string merged_preferences;
  if (!base::JSONWriter::Write(*ash_root, &merged_preferences)) {
    PLOG(ERROR) << "Failure while generating resulting Preferences JSON";
    return false;
  }

  // Write the resulting JSON to disk.
  if (!base::WriteFile(tmp_pref_path, merged_preferences)) {
    PLOG(ERROR) << "Failure while writing Preferences JSON to "
                << tmp_pref_path.value();
    return false;
  }

  return true;
}

// static
bool BrowserDataBackMigrator::MergeLacrosPreferences(
    base::Value::Dict& ash_root_dict,
    const std::vector<std::string>& path,
    const base::Value& current_value) {
  if (path.size() >= kMaxRecursionDepth) {
    LOG(WARNING) << "We have reached maximum recursion depth "
                 << kMaxRecursionDepth
                 << " and we are stopping MergeLacrosPreferences()";
    return false;
  }

  // If the |path| was split or ash-only, then ignore it.
  // For predefined path, each component should not contain '.' so filter
  // them out to avoid finding wrong paths.
  if (base::ranges::all_of(path, [](const std::string& component) {
        return component.find('.') == std::string::npos;
      })) {
    const std::string dotted_path = base::JoinString(path, ".");
    if (base::Contains(browser_data_migrator_util::kSplitPreferencesKeys,
                       dotted_path) ||
        base::Contains(browser_data_migrator_util::kAshOnlyPreferencesKeys,
                       dotted_path)) {
      return true;
    }
  }

  // If current value is not a dictionary, then it is a final pref.
  // Merge it into the |ash_root_dict|.
  if (!current_value.is_dict()) {
    // Guaranteed by the caller, that first value is a dict.
    DCHECK(!path.empty());

    base::Value::Dict* dict = &ash_root_dict;
    for (const auto& key :
         base::span<const std::string>(path.begin(), path.size() - 1)) {
      base::Value* child = dict->Find(key);
      dict = child ? child->GetIfDict() : dict->EnsureDict(key);
      if (!dict) {
        // There's an non-dict entry at non-last component of the path.
        // Fail here. This behavior is to be compatible with SetDottedPath(),
        // which is used in the orignal code not to change the detailed
        // behavior for urgent fix.
        return false;
      }
    }
    dict->Set(path.back(), current_value.Clone());
    return true;
  }

  // Otherwise, traverse all child elements of the current dictionary.
  std::vector<std::string> child_path = path;
  for (const auto [child_key, child_value] : current_value.GetDict()) {
    child_path.push_back(child_key);
    if (!MergeLacrosPreferences(ash_root_dict, child_path, child_value)) {
      return false;
    }
    child_path.pop_back();
  }

  return true;
}

// static
bool BrowserDataBackMigrator::IsLacrosOnlyExtension(
    std::string_view extension_id) {
  return !base::Contains(browser_data_migrator_util::kExtensionsAshOnly,
                         extension_id) &&
         !base::Contains(
             extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser(),
             extension_id);
}

// static
bool BrowserDataBackMigrator::CopyLevelDBBase(
    const base::FilePath& lacros_leveldb_dir,
    const base::FilePath& tmp_leveldb_dir) {
  if (!base::CopyDirectory(lacros_leveldb_dir, tmp_leveldb_dir,
                           true /* recursive */)) {
    PLOG(ERROR) << "CopyDirectory() failed from " << lacros_leveldb_dir.value()
                << " to " << tmp_leveldb_dir.value();
    return false;
  }
  return true;
}

// static
bool BrowserDataBackMigrator::MergeLevelDB(
    const base::FilePath& ash_db_path,
    const base::FilePath& tmp_db_path,
    const browser_data_migrator_util::LevelDBType leveldb_type) {
  // If the Ash database does not exist we do not need to merge anything. We are
  // sure that the Lacros database exists in the temporary directory at this
  // step, otherwise the `CopyLevelDBBase` call would have already failed.
  if (!base::PathExists(ash_db_path)) {
    LOG(WARNING) << "Only Lacros LevelDB exists, not " << ash_db_path.value();
    return true;
  }

  // Both databases exist so we need to merge them.
  leveldb_env::Options options;
  options.create_if_missing = false;

  // Open Ash LevelDB database.
  std::unique_ptr<leveldb::DB> ash_db;
  leveldb::Status status =
      leveldb_env::OpenDB(options, ash_db_path.value(), &ash_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening Ash leveldb: " << ash_db_path;
    return false;
  }

  // Open temporary LevelDB database, which is the copy of Lacros one.
  std::unique_ptr<leveldb::DB> tmp_db;
  status = leveldb_env::OpenDB(options, tmp_db_path.value(), &tmp_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening Lacros leveldb: " << tmp_db_path;
    return false;
  }

  // Retrieve all extensions' keys, indexed by extension id.
  browser_data_migrator_util::ExtensionKeys ash_keys;
  status = browser_data_migrator_util::GetExtensionKeys(
      ash_db.get(), leveldb_type, &ash_keys);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while reading keys from Ash leveldb: "
                << ash_db_path;
    return false;
  }

  // Collect all necessary changes to be written in a batch.
  leveldb::WriteBatch write_batch;
  for (const auto& [extension_id, keys] : ash_keys) {
    if (!IsLacrosOnlyExtension(extension_id)) {
      for (const std::string& key : keys) {
        std::string value;
        status = ash_db->Get(leveldb::ReadOptions(), key, &value);
        if (!status.ok()) {
          PLOG(ERROR) << "Failure while reading from Ash leveldb: "
                      << ash_db_path;
          return false;
        }
        write_batch.Put(key, value);
      }
    }
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = tmp_db->Write(write_options, &write_batch);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while writing into new leveldb: "
                << tmp_db_path.value();
    return false;
  }

  return true;
}

// static
bool BrowserDataBackMigrator::MergeSyncDataLevelDB(
    const base::FilePath& ash_db_path,
    const base::FilePath& lacros_db_path,
    const base::FilePath& tmp_db_path) {
  // Create a directory for the result database.
  if (!base::CreateDirectory(tmp_db_path.DirName().DirName()) ||
      !base::CreateDirectory(tmp_db_path.DirName())) {
    PLOG(ERROR) << "CreateDirectory() for " << tmp_db_path.value()
                << " failed.";
    return false;
  }

  // If only one of the databases exists we do not need to merge anything and
  // can just copy the database to the temp directory.
  if (base::PathExists(ash_db_path) && !base::PathExists(lacros_db_path)) {
    LOG(WARNING) << "Only Ash Sync Data LevelDB exists.";
    if (!base::CopyFile(ash_db_path, tmp_db_path)) {
      PLOG(ERROR) << "Failure to copy Ash Sync Data LevelDB: "
                  << ash_db_path.value() << " to " << tmp_db_path.value();
      return false;
    }
    return true;
  }

  if (!base::PathExists(ash_db_path) && base::PathExists(lacros_db_path)) {
    LOG(WARNING) << "Only Lacros Sync Data LevelDB exists.";
    if (!base::CopyFile(lacros_db_path, tmp_db_path)) {
      PLOG(ERROR) << "Failure to copy Lacros Sync Data LevelDB: "
                  << lacros_db_path.value() << " to " << tmp_db_path.value();
      return false;
    }
    return true;
  }

  // Both databases exist so we need to merge them.
  leveldb_env::Options options;
  options.create_if_missing = false;

  // Open Ash LevelDB database.
  std::unique_ptr<leveldb::DB> ash_db;
  leveldb::Status status =
      leveldb_env::OpenDB(options, ash_db_path.value(), &ash_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening Ash Sync Data LevelDB: "
                << ash_db_path.value();
    return false;
  }

  // Open Lacros LevelDB database.
  std::unique_ptr<leveldb::DB> lacros_db;
  status = leveldb_env::OpenDB(options, lacros_db_path.value(), &lacros_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening Lacros Sync Data LevelDB: "
                << lacros_db_path.value();
    return false;
  }

  // Open the result LevelDB database.
  std::unique_ptr<leveldb::DB> result_db;
  options.create_if_missing = true;
  options.error_if_exists = true;
  status = leveldb_env::OpenDB(options, tmp_db_path.value(), &result_db);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while opening result Sync Data LevelDB: "
                << tmp_db_path;
    return false;
  }

  // Get Ash Sync Data types from the Ash database.
  leveldb::WriteBatch ash_write_batch;
  {
    std::unique_ptr<leveldb::Iterator> it(
        ash_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      const std::string key = it->key().ToString();
      const std::string value = it->value().ToString();
      if (browser_data_migrator_util::IsAshOnlySyncDataType(key))
        ash_write_batch.Put(key, value);
    }
    if (!it->status().ok()) {
      PLOG(ERROR) << "Failure while reading from Ash Sync Data LevelDB: "
                  << ash_db_path;
      return false;
    }
  }

  // Get Lacros Sync Data types from the Lacros database.
  leveldb::WriteBatch lacros_write_batch;
  {
    std::unique_ptr<leveldb::Iterator> it(
        lacros_db->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      const std::string key = it->key().ToString();
      const std::string value = it->value().ToString();
      if (!browser_data_migrator_util::IsAshOnlySyncDataType(key))
        lacros_write_batch.Put(key, value);
    }
    if (!it->status().ok()) {
      PLOG(ERROR) << "Failure while reading from Lacros Sync Data LevelDB: "
                  << lacros_db_path;
      return false;
    }
  }

  // Merge all the data types into the resulting database.
  leveldb::WriteOptions write_options;
  write_options.sync = true;

  // Write Lacros data first, i.e. prefer Ash data if there are duplicates.
  status = result_db->Write(write_options, &lacros_write_batch);
  if (!status.ok()) {
    PLOG(ERROR)
        << "Failure while writing Lacros Sync Data into result database: "
        << tmp_db_path;
    return false;
  }
  status = result_db->Write(write_options, &ash_write_batch);
  if (!status.ok()) {
    PLOG(ERROR) << "Failure while writing Ash Sync Data into result database: "
                << tmp_db_path;
    return false;
  }

  return true;
}

// static
bool BrowserDataBackMigrator::IsBackMigrationForceEnabled() {
  const std::string force_migration_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceBrowserDataBackwardMigration);

  return force_migration_switch == kBrowserDataBackwardMigrationForceMigration;
}

// static
bool BrowserDataBackMigrator::IsBackMigrationEnabled(
    ash::standalone_browser::migrator_util::PolicyInitState policy_init_state) {
  if (IsBackMigrationForceEnabled()) {
    VLOG(1) << "Lacros backward migration is force enabled";
    return true;
  }

  // Check if migration should be force skipped.
  const std::string force_migration_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceBrowserDataBackwardMigration);

  if (force_migration_switch == kBrowserDataBackwardMigrationForceSkip) {
    VLOG(1) << "Lacros backward migration is force skipped";
    return false;
  }

  crosapi::browser_util::LacrosDataBackwardMigrationMode migration_mode =
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kNone;
  if (policy_init_state ==
      ash::standalone_browser::migrator_util::PolicyInitState::kBeforeInit) {
    std::optional<crosapi::browser_util::LacrosDataBackwardMigrationMode>
        parsed = std::nullopt;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            crosapi::browser_util::
                kLacrosDataBackwardMigrationModePolicySwitch)) {
      parsed = crosapi::browser_util::ParseLacrosDataBackwardMigrationMode(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              crosapi::browser_util::
                  kLacrosDataBackwardMigrationModePolicySwitch));
    }

    migration_mode =
        parsed.has_value()
            ? parsed.value()
            : crosapi::browser_util::LacrosDataBackwardMigrationMode::kNone;
  } else {
    DCHECK_EQ(
        policy_init_state,
        ash::standalone_browser::migrator_util::PolicyInitState::kAfterInit);
    migration_mode =
        crosapi::browser_util::GetCachedLacrosDataBackwardMigrationMode();
  }

  // Backward migration can be explicitly enabled by using the
  // LacrosDataBackwardMigrationMode policy.
  if (migration_mode ==
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kKeepAll) {
    VLOG(1) << "Lacros backward migration mode is keep_all";
    return true;
  }

  // Modes beside none do not go through backward migration.
  // None is the default, fall back to the feature instead.
  if (migration_mode !=
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kNone) {
    VLOG(1) << "Lacros backward migration mode is not none";
    return false;
  }

  bool is_feature_enabled = base::FeatureList::IsEnabled(
      ash::features::kLacrosProfileBackwardMigration);
  VLOG(1) << "Lacros backward migration feature flag is " << is_feature_enabled;
  return is_feature_enabled;
}

// static
bool BrowserDataBackMigrator::ShouldMigrateBack(
    const AccountId& account_id,
    const std::string& user_id_hash,
    ash::standalone_browser::migrator_util::PolicyInitState policy_init_state) {
  if (IsBackMigrationForceEnabled()) {
    LOG(WARNING) << "Lacros backward migration has been force enabled";
    // Skipping other checks, except for lacros folder presence.
  } else {
    // Check if the backward migration is enabled.
    if (!IsBackMigrationEnabled(policy_init_state)) {
      VLOG(1) << "Lacros backward migration is disabled, not triggering";
      return false;
    }

    const user_manager::User* user =
        user_manager::UserManager::Get()->FindUser(account_id);

    if (!user) {
      VLOG(1) << "Failed to find user, not triggering backward migration";
      return false;
    }

    // Backward migration should not run for secondary users.
    const auto* primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    // `ShouldMigrateBack()` is called from `MaybeRestartToMigrateBack()`, which
    // is called either before or after profile initialization. In the former
    // case it is called from `PreProfileInit()` and this is only called for the
    // primary profile so we can assume that the user is the primary user if
    // `primary_user == nullptr`. If primary_user is not null then we check if
    // `user != primary_user`.
    if (primary_user && (user != primary_user)) {
      VLOG(1) << "Skip backward migration for secondary users.";
      return false;
    }

    if (crosapi::browser_util::IsLacrosEnabledForMigration(user,
                                                           policy_init_state)) {
      VLOG(1) << "Lacros is enabled, not triggering backward migration";
      return false;
    }
  }

  // Forced migration still needs the lacros dir to be present. Otherwise
  // we will continuously migrate until the flag is removed.
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Could not get the original user data dir path. Not "
                  "triggering backward migration";
    return false;
  }

  const base::FilePath ash_data_dir =
      user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));

  const base::FilePath lacros_dir =
      ash_data_dir.Append(browser_data_migrator_util::kLacrosDir);

  {
    // Temporarily allow blocking since we need to check if we need to migrate
    // the data from lacros to ash before the user has a chance to use it.
    base::ScopedAllowBlocking allow_blocking;

    // Synchronously check if the lacros folder is present.
    if (!DirectoryExists(lacros_dir)) {
      VLOG(1) << "Lacros folder not found at '" << lacros_dir.value()
              << "', not triggering backward migration";
      return false;
    }
  }

  return true;
}

// static
bool BrowserDataBackMigrator::RestartToMigrateBack(
    const AccountId& account_id) {
  LOG(WARNING) << "Requesting backward migration from session_manager";
  bool success =
      SessionManagerClient::Get()->BlockingRequestBrowserDataBackwardMigration(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id));

  if (!success) {
    LOG(ERROR) << "SessionManagerClient::"
                  "BlockingRequestBrowserDataBackwardMigration() failed.";
    return false;
  }

  AttemptRestart();
  return true;
}

// static
BrowserDataBackMigratorBase::Result BrowserDataBackMigrator::ToResult(
    TaskResult result) {
  switch (result.status) {
    case TaskStatus::kSucceeded:
      return Result::kSucceeded;
    case TaskStatus::kPreMigrationCleanUpDeleteTmpDirFailed:
    case TaskStatus::kMergeSplitItemsCreateTmpDirFailed:
    case TaskStatus::kMergeSplitItemsCopyExtensionsFailed:
    case TaskStatus::kMergeSplitItemsCopyExtensionStorageFailed:
    case TaskStatus::kMergeSplitItemsCreateDirFailed:
    case TaskStatus::kMergeSplitItemsMergeIndexedDBFailed:
    case TaskStatus::kMergeSplitItemsMergePrefsFailed:
    case TaskStatus::kMergeSplitItemsMergeLocalStorageLevelDBFailed:
    case TaskStatus::kMergeSplitItemsMergeStateStoreLevelDBFailed:
    case TaskStatus::kMergeSplitItemsMergeSyncDataFailed:
    case TaskStatus::kDeleteAshItemsDeleteExtensionsFailed:
    case TaskStatus::kDeleteAshItemsDeleteLacrosItemFailed:
    case TaskStatus::kDeleteLacrosDirDeleteFailed:
    case TaskStatus::kDeleteTmpDirDeleteFailed:
    case TaskStatus::kMoveLacrosItemsToAshDirFailed:
    case TaskStatus::kMoveMergedItemsBackToAshCopyDirectoryFailed:
    case TaskStatus::kMoveMergedItemsBackToAshMoveFileFailed:
      return Result::kFailed;
  }
}

void BrowserDataBackMigrator::InvokeCallback(TaskResult result) {
  browser_data_back_migrator_metrics::RecordFinalStatus(result);
  browser_data_back_migrator_metrics::RecordPosixErrnoIfAvailable(result);
  browser_data_back_migrator_metrics::RecordMigrationTimeIfSuccessful(
      result, migration_start_time_);
  std::move(finished_callback_).Run(ToResult(result));
}

}  // namespace ash
