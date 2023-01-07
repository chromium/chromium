// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/move_migrator.h"

#include <errno.h>

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/migration_progress_tracker.h"
#include "chrome/common/chrome_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {

MoveMigrator::MoveMigrator(
    const base::FilePath& original_profile_dir,
    const std::string& user_id_hash,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag,
    PrefService* local_state,
    MigrationFinishedCallback finished_callback)
    : original_profile_dir_(original_profile_dir),
      user_id_hash_(user_id_hash),
      progress_tracker_(std::move(progress_tracker)),
      cancel_flag_(cancel_flag),
      local_state_(local_state),
      finished_callback_(std::move(finished_callback)) {
  DCHECK(local_state_);
}

MoveMigrator::~MoveMigrator() = default;

// TODO(ythjkt): Add UMA for each step to detect failures and measure time taken
// for critical steps.
void MoveMigrator::Migrate() {
  ResumeStep resume_step = GetResumeStep(local_state_, user_id_hash_);

  if (IsResumeStep(resume_step)) {
    const int resume_count =
        UpdateResumeAttemptCountForUser(local_state_, user_id_hash_);

    if (resume_count > 0) {
      LOG(WARNING) << "Resuming move migration for the " << resume_count
                   << "th time.";
      UMA_HISTOGRAM_COUNTS_100(kMoveMigratorResumeCount, resume_count);
    }

    if (resume_count > kMoveMigrationResumeCountLimit) {
      LOG(ERROR) << "The number of resume attempt limit has reached. Marking "
                    "move migration as completed.";
      SetResumeStep(local_state_, user_id_hash_, ResumeStep::kCompleted);
      resume_step = ResumeStep::kCompleted;
    }
  }

  // Start or resume migration.
  UMA_HISTOGRAM_ENUMERATION(kMoveMigratorResumeStepUMA, resume_step);
  switch (resume_step) {
    case ResumeStep::kStart:
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&MoveMigrator::PreMigrationCleanUp,
                         original_profile_dir_),
          base::BindOnce(&MoveMigrator::OnPreMigrationCleanUp,
                         weak_factory_.GetWeakPtr()));
      return;
    case ResumeStep::kMoveLacrosItems:
      LOG(ERROR) << "Migration did not complete in the previous attempt. "
                    "Resuming migration from kMoveLacrosItems step.";
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&MoveMigrator::MoveLacrosItemsToNewDir,
                         original_profile_dir_),
          base::BindOnce(&MoveMigrator::OnMoveLacrosItemsToNewDir,
                         weak_factory_.GetWeakPtr()));
      return;
    case ResumeStep::kMoveSplitItems:
      LOG(ERROR) << "Migration did not complete in the previous attempt. "
                    "Resuming migration from kMoveSplitItems step.";
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&MoveMigrator::MoveSplitItemsToOriginalDir,
                         original_profile_dir_),
          base::BindOnce(&MoveMigrator::OnMoveSplitItemsToOriginalDir,
                         weak_factory_.GetWeakPtr()));
      return;
    case ResumeStep::kMoveTmpDir:
      LOG(ERROR) << "Migration did not complete in the previous attempt. "
                    "Resuming migration from kMoveTmpDir step.";
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&MoveMigrator::MoveTmpDirToLacrosDir,
                         original_profile_dir_),
          base::BindOnce(&MoveMigrator::OnMoveTmpDirToLacrosDir,
                         weak_factory_.GetWeakPtr()));
      return;
    case ResumeStep::kCompleted:
      LOG(ERROR)
          << "This state indicates that migration was marked as completed by"
             "`MoveMigrator` but was not by `BrowserDataMigratorImpl`";
      InvokeCallback({TaskStatus::kSucceeded});
      return;
  }
}

// static
bool MoveMigrator::ResumeRequired(PrefService* local_state,
                                  const std::string& user_id_hash) {
  ResumeStep resume_step = GetResumeStep(local_state, user_id_hash);

  return IsResumeStep(resume_step);
}

bool MoveMigrator::IsResumeStep(ResumeStep resume_step) {
  switch (resume_step) {
    case ResumeStep::kStart:
      return false;
    case ResumeStep::kMoveLacrosItems:
      return true;
    case ResumeStep::kMoveSplitItems:
      return true;
    case ResumeStep::kMoveTmpDir:
      return true;
    case ResumeStep::kCompleted:
      return false;
  }
}

// static
void MoveMigrator::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMoveMigrationResumeStepPref,
                                   base::Value(base::Value::Type::DICT));
  registry->RegisterDictionaryPref(kMoveMigrationResumeCountPref,
                                   base::Value(base::Value::Type::DICT));
}

// static
MoveMigrator::ResumeStep MoveMigrator::GetResumeStep(
    PrefService* local_state,
    const std::string& user_id_hash) {
  return static_cast<ResumeStep>(
      local_state->GetDict(kMoveMigrationResumeStepPref)
          .FindInt(user_id_hash)
          .value_or(0));
}

// static
void MoveMigrator::SetResumeStep(PrefService* local_state,
                                 const std::string& user_id_hash,
                                 const ResumeStep step) {
  ScopedDictPrefUpdate update(local_state, kMoveMigrationResumeStepPref);
  base::Value::Dict& dict = update.Get();
  dict.Set(user_id_hash, static_cast<int>(step));
  local_state->CommitPendingWrite();
}

int MoveMigrator::UpdateResumeAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  int count = local_state->GetDict(kMoveMigrationResumeCountPref)
                  .FindIntByDottedPath(user_id_hash)
                  .value_or(0);
  count += 1;
  ScopedDictPrefUpdate update(local_state, kMoveMigrationResumeCountPref);
  base::Value::Dict& dict = update.Get();
  dict.Set(user_id_hash, count);
  return count;
}

void MoveMigrator::ClearResumeAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  ScopedDictPrefUpdate update(local_state, kMoveMigrationResumeCountPref);
  base::Value::Dict& dict = update.Get();
  dict.Remove(user_id_hash);
}

// static
void MoveMigrator::ClearResumeStepForUser(PrefService* local_state,
                                          const std::string& user_id_hash) {
  ScopedDictPrefUpdate update(local_state, kMoveMigrationResumeStepPref);
  base::Value::Dict& dict = update.Get();
  dict.Remove(user_id_hash);
}

// static
MoveMigrator::TaskResult MoveMigrator::PreMigrationCleanUp(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running PreMigrationCleanUp()";
  base::ElapsedTimer timer;

  const base::FilePath new_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  if (base::PathExists(new_user_dir)) {
    // Delete an existing lacros profile before the migration.
    if (!base::DeletePathRecursively(new_user_dir)) {
      PLOG(ERROR) << "Deleting " << new_user_dir.value() << " failed: ";
      return {TaskStatus::kPreMigrationCleanUpDeleteLacrosDirFailed, errno};
    }
  }

  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  if (base::PathExists(tmp_user_dir)) {
    // Delete tmp_user_dir if any were left from a previous failed move
    // migration attempt. Note that if resuming move migration from later steps
    // such as `MoveLacrosItemsToNewDir()`, this tmp_user_dir will not be
    // deleted. This is an intended behaviour because we do not want to delete
    // tmp_user_dir once we start deleting items from the Ash PDD.
    if (!base::DeletePathRecursively(tmp_user_dir)) {
      PLOG(ERROR) << "Deleting " << tmp_user_dir.value() << " failed: ";
      return {TaskStatus::kPreMigrationCleanUpDeleteTmpDirFailed, errno};
    }
  }

  const base::FilePath tmp_split_dir =
      original_profile_dir.Append(browser_data_migrator_util::kSplitTmpDir);
  if (base::PathExists(tmp_split_dir)) {
    // Delete tmp_split_dir if any were left from a previous failed move
    // migration attempt. Similar considerations to tmp_user_dir apply.
    if (!base::DeletePathRecursively(tmp_split_dir)) {
      PLOG(ERROR) << "Deleting" << tmp_split_dir.value() << " failed: ";
      return {TaskStatus::kPreMigrationCleanUpDeleteTmpSplitDirFailed, errno};
    }
  }

  // Delete deletable items to free up space.
  browser_data_migrator_util::TargetItems deletable_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir,
          browser_data_migrator_util::ItemType::kDeletable);
  for (const auto& item : deletable_items.items) {
    bool result = item.is_directory ? base::DeletePathRecursively(item.path)
                                    : base::DeleteFile(item.path);
    if (!result) {
      // This is not critical to the migration so log the error but do stop the
      // migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  // Now check if there is enough disk space for the migration to be carried
  // out.
  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir,
          browser_data_migrator_util::ItemType::kNeedCopyForMove);

  const int64_t extra_bytes_required_to_be_freed =
      browser_data_migrator_util::ExtraBytesRequiredToBeFreed(
          need_copy_items.total_size, original_profile_dir);

  UMA_HISTOGRAM_MEDIUM_TIMES(kMoveMigratorPreMigrationCleanUpTimeUMA,
                             timer.Elapsed());
  if (extra_bytes_required_to_be_freed > 0u) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(kMoveMigratorExtraSpaceRequiredMB,
                                extra_bytes_required_to_be_freed / 1024 / 1024,
                                1, 10000, 100);
    LOG(ERROR) << "Not enough disk space available to carry out the migration "
                  "safely. Need to free up "
               << extra_bytes_required_to_be_freed << " bytes from "
               << original_profile_dir;
    return {TaskStatus::kPreMigrationCleanUpNotEnoughSpace, absl::nullopt,
            extra_bytes_required_to_be_freed};
  }

  return {TaskStatus::kSucceeded};
}

void MoveMigrator::OnPreMigrationCleanUp(MoveMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "PreMigrationCleanup() failed.";
    InvokeCallback(result);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&MoveMigrator::SetupLacrosDir, original_profile_dir_,
                     std::move(progress_tracker_), cancel_flag_),
      base::BindOnce(&MoveMigrator::OnSetupLacrosDir,
                     weak_factory_.GetWeakPtr()));
}

// static
MoveMigrator::TaskResult MoveMigrator::SetupLacrosDir(
    const base::FilePath& original_profile_dir,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag) {
  LOG(WARNING) << "Running SetupLacrosDir()";
  base::ElapsedTimer timer;

  if (cancel_flag->IsSet()) {
    LOG(WARNING) << "Migration is cancelled.";
    return {TaskStatus::kCancelled};
  }
  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);

  if (!base::CreateDirectory(tmp_user_dir)) {
    PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_user_dir.value();
    return {TaskStatus::kSetupLacrosDirCreateTmpDirFailed, errno};
  }
  if (!base::CreateDirectory(tmp_profile_dir)) {
    PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_profile_dir.value();
    return {TaskStatus::kSetupLacrosDirCreateTmpProfileDirFailed, errno};
  }

  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir,
          browser_data_migrator_util::ItemType::kNeedCopyForMove);

  progress_tracker->SetTotalSizeToCopy(need_copy_items.total_size);

  base::ElapsedTimer timer_for_copy;
  if (!browser_data_migrator_util::CopyTargetItems(
          tmp_profile_dir, need_copy_items, cancel_flag.get(),
          progress_tracker.get())) {
    if (cancel_flag->IsSet()) {
      return {TaskStatus::kCancelled};
    }
    LOG(ERROR) << "CopyTargetItems() failed for need_copy_items.";
    return {TaskStatus::kSetupLacrosDirCopyTargetItemsFailed, errno};
  }
  UMA_HISTOGRAM_MEDIUM_TIMES(kMoveMigratorSetupLacrosDirCopyTargetItemsTimeUMA,
                             timer_for_copy.Elapsed());

  if (!base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel), "")) {
    LOG(ERROR) << "WriteFile() failed for " << chrome::kFirstRunSentinel;
    return {TaskStatus::kSetupLacrosDirWriteFirstRunSentinelFileFailed, errno};
  }

  return {TaskStatus::kSucceeded};
}

void MoveMigrator::OnSetupLacrosDir(TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MoveMigrator::SetupLacrosDir() failed.";
    if (cancel_flag_->IsSet()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(kMoveMigratorCancelledMigrationTimeUMA,
                                 timer_.Elapsed());
    }
    InvokeCallback(result);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&MoveMigrator::SetupAshSplitDir, original_profile_dir_),
      base::BindOnce(&MoveMigrator::OnSetupAshSplitDir,
                     weak_factory_.GetWeakPtr()));
}

// static
MoveMigrator::TaskResult MoveMigrator::SetupAshSplitDir(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running SetupAshSplitDir()";

  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);
  const base::FilePath tmp_split_dir =
      original_profile_dir.Append(browser_data_migrator_util::kSplitTmpDir);
  if (!base::CreateDirectory(tmp_split_dir)) {
    PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_split_dir.value();
    return {TaskStatus::kSetupAshDirCreateSplitDirFailed, errno};
  }

  // Copy extensions that have to be in both Ash and Lacros.
  TaskResult task_result =
      CopyBothChromesSubdirs(original_profile_dir, tmp_split_dir,
                             browser_data_migrator_util::kExtensionsFilePath,
                             TaskStatus::kSetupAshDirCopyExtensionsFailed);
  if (task_result.status != TaskStatus::kSucceeded)
    return task_result;

  // Copy Storage objects for extensions that have to be both in Ash and Lacros.
  task_result = CopyBothChromesSubdirs(
      original_profile_dir, tmp_split_dir,
      base::FilePath(browser_data_migrator_util::kStorageFilePath)
          .Append(browser_data_migrator_util::kStorageExtFilePath)
          .value(),
      TaskStatus::kSetupAshDirCopyStorageFailed);
  if (task_result.status != TaskStatus::kSucceeded)
    return task_result;

  // Copy IndexedDB objects for extensions that have to be both in
  // Ash and Lacros.
  const base::FilePath original_indexed_db_dir = original_profile_dir.Append(
      browser_data_migrator_util::kIndexedDBFilePath);
  if (base::PathExists(original_indexed_db_dir)) {
    const base::FilePath split_indexed_db_dir =
        tmp_split_dir.Append(browser_data_migrator_util::kIndexedDBFilePath);
    if (!base::CreateDirectory(split_indexed_db_dir)) {
      PLOG(ERROR) << "CreateDirectory() failed for "
                  << split_indexed_db_dir.value();
      return {TaskStatus::kSetupAshDirCreateDirFailed, errno};
    }

    for (const char* extension_id :
         browser_data_migrator_util::kExtensionsBothChromes) {
      if (!browser_data_migrator_util::MigrateAshIndexedDB(
              original_profile_dir, split_indexed_db_dir, extension_id,
              /*copy=*/true)) {
        return {TaskStatus::kSetupAshDirCopyIndexedDBFailed, errno};
      }
    }
  }

  // Create Ash's version of `Local Storage`, holding *only* the keys
  // associated to the extensions that have to stay in Ash.
  if (base::PathExists(
          original_profile_dir
              .Append(browser_data_migrator_util::kLocalStorageFilePath)
              .Append(browser_data_migrator_util::kLocalStorageLeveldbName))) {
    if (!browser_data_migrator_util::MigrateLevelDB(
            original_profile_dir
                .Append(browser_data_migrator_util::kLocalStorageFilePath)
                .Append(browser_data_migrator_util::kLocalStorageLeveldbName),
            tmp_split_dir
                .Append(browser_data_migrator_util::kLocalStorageFilePath)
                .Append(browser_data_migrator_util::kLocalStorageLeveldbName),
            browser_data_migrator_util::LevelDBType::kLocalStorage)) {
      LOG(ERROR) << "MigrateLevelDB() failed for `Local Storage`";
      return {TaskStatus::kSetupAshDirMigrateLevelDBForLocalStateFailed};
    }
  }

  // Create Ash's version of all the state stores (Extension State, etc.).
  for (const char* path : browser_data_migrator_util::kStateStorePaths) {
    if (base::PathExists(original_profile_dir.Append(path))) {
      if (!browser_data_migrator_util::MigrateLevelDB(
              original_profile_dir.Append(path), tmp_split_dir.Append(path),
              browser_data_migrator_util::LevelDBType::kStateStore)) {
        LOG(ERROR) << "MigrateLevelDB() failed for `" << path << "`";
        return {TaskStatus::kSetupAshDirMigrateLevelDBForStateFailed};
      }
    }
  }

  // Split Preferences.
  if (!browser_data_migrator_util::MigratePreferences(
          original_profile_dir.Append(chrome::kPreferencesFilename),
          tmp_split_dir.Append(chrome::kPreferencesFilename),
          tmp_profile_dir.Append(chrome::kPreferencesFilename))) {
    LOG(ERROR) << "MigratePreferences() failed.";
    return {TaskStatus::kSetupAshDirMigratePreferencesFailed};
  }

  // Split Sync Data.
  if (base::PathExists(
          original_profile_dir
              .Append(browser_data_migrator_util::kSyncDataFilePath)
              .Append(browser_data_migrator_util::kSyncDataLeveldbName))) {
    if (!browser_data_migrator_util::MigrateSyncDataLevelDB(
            original_profile_dir
                .Append(browser_data_migrator_util::kSyncDataFilePath)
                .Append(browser_data_migrator_util::kSyncDataLeveldbName),
            tmp_split_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
                .Append(browser_data_migrator_util::kSyncDataLeveldbName),
            tmp_profile_dir
                .Append(browser_data_migrator_util::kSyncDataFilePath)
                .Append(browser_data_migrator_util::kSyncDataLeveldbName))) {
      LOG(ERROR) << "MigrateSyncDataLevelDB() failed";
      return {TaskStatus::kSetupAshDirMigrateSyncDataLevelDBFailed};
    }
  }

  return {TaskStatus::kSucceeded};
}

void MoveMigrator::OnSetupAshSplitDir(TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MoveMigrator::SetupAshSplitDir() failed.";
    InvokeCallback(result);
    return;
  }

  // Once `MoveLacrosItemsToNewDir()` is started, it should be completed.
  // Otherwise the profile in ash directory becomes fragmented. We store the
  // resume step as `kMoveLacrosItems` in Local State so that if the migration
  // is interrupted during `MoveLacrosItemsToNewDir()` then the migrator can
  // resume the migration from that point.
  SetResumeStep(local_state_, user_id_hash_, ResumeStep::kMoveLacrosItems);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&MoveMigrator::MoveLacrosItemsToNewDir,
                     original_profile_dir_),
      base::BindOnce(&MoveMigrator::OnMoveLacrosItemsToNewDir,
                     weak_factory_.GetWeakPtr()));
}

// static
MoveMigrator::TaskResult MoveMigrator::MoveLacrosItemsToNewDir(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running MoveLacrosItemsToNewDir()";

  base::ElapsedTimer timer;

  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir, browser_data_migrator_util::ItemType::kLacros);

  for (const auto& item : lacros_items.items) {
    if (item.is_directory && !base::PathIsWritable(item.path)) {
      PLOG(ERROR) << "The current process does not have write permission to "
                     "the directory "
                  << item.path.value();
      return {TaskStatus::kMoveLacrosItemsToNewDirNoWritePerm, errno};
    }
  }

  const base::FilePath tmp_profile_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir)
          .Append(browser_data_migrator_util::kLacrosProfilePath);

  // Nigori file needs special handling, because it's not stored directly under
  // |original_profile_dir|.
  const base::FilePath original_nigori_path =
      original_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
          .Append(browser_data_migrator_util::kSyncDataNigoriFileName);
  if (base::PathExists(original_nigori_path)) {
    // In theory, `Sync Data` directory should be already created by
    // SplitSyncDataLevelDB() as long as DB exists. It still needs to be created
    // manually here, to handle the case when DB doesn't exist, but Nigori file
    // does.
    if (!base::CreateDirectory(tmp_profile_dir.Append(
            browser_data_migrator_util::kSyncDataFilePath))) {
      PLOG(ERROR) << "Failure while creating "
                  << tmp_profile_dir.Append(
                         browser_data_migrator_util::kSyncDataFilePath)
                  << " directory.";
      return {TaskStatus::kMoveLacrosItemsCreateDirFailed, errno};
    }

    const base::FilePath target_nigori_path =
        tmp_profile_dir.Append(browser_data_migrator_util::kSyncDataFilePath)
            .Append(browser_data_migrator_util::kSyncDataNigoriFileName);
    if (!base::Move(original_nigori_path, target_nigori_path)) {
      PLOG(ERROR) << "Failed to move item " << original_nigori_path.value()
                  << " to " << target_nigori_path << ": ";
      return {TaskStatus::kMoveLacrosItemsToNewDirMoveFailed, errno};
    }
  }

  for (const auto& item : lacros_items.items) {
    if (!base::Move(item.path, tmp_profile_dir.Append(item.path.BaseName()))) {
      PLOG(ERROR) << "Failed to move item " << item.path.value() << " to "
                  << tmp_profile_dir.Append(item.path.BaseName()) << ": ";
      return {TaskStatus::kMoveLacrosItemsToNewDirMoveFailed, errno};
    }
  }

  UMA_HISTOGRAM_MEDIUM_TIMES(kMoveMigratorMoveLacrosItemsTimeUMA,
                             timer.Elapsed());
  return {TaskStatus::kSucceeded};
}

void MoveMigrator::OnMoveLacrosItemsToNewDir(TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "Moving Lacros items to temporary directory failed.";
    InvokeCallback(result);
    return;
  }

  SetResumeStep(local_state_, user_id_hash_, ResumeStep::kMoveSplitItems);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&MoveMigrator::MoveSplitItemsToOriginalDir,
                     original_profile_dir_),
      base::BindOnce(&MoveMigrator::OnMoveSplitItemsToOriginalDir,
                     weak_factory_.GetWeakPtr()));
}

// static
MoveMigrator::TaskResult MoveMigrator::MoveSplitItemsToOriginalDir(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running MoveSplitItemsToOriginalDir()";

  const base::FilePath tmp_split_dir =
      original_profile_dir.Append(browser_data_migrator_util::kSplitTmpDir);
  const base::FilePath tmp_profile_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir)
          .Append(browser_data_migrator_util::kLacrosProfilePath);

  // Move everything inside tmp_split_dir to Ash's profile directory.
  base::FileEnumerator e(
      tmp_split_dir, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);

  for (base::FilePath path = e.Next(); !path.empty(); path = e.Next()) {
    base::FilePath ash_path = original_profile_dir.Append(path.BaseName());
    if (base::DirectoryExists(ash_path) && !DeletePathRecursively(ash_path)) {
      PLOG(ERROR) << "Failed deleting " << ash_path.value();
      return {TaskStatus::kMoveSplitItemsToOriginalDirMoveSplitItemsFailed,
              errno};
    }

    if (!base::Move(path, ash_path)) {
      PLOG(ERROR) << "Failed moving " << path.value() << " to "
                  << ash_path.value();
      return {TaskStatus::kMoveSplitItemsToOriginalDirMoveSplitItemsFailed,
              errno};
    }
  }

  // Delete tmp_split_dir.
  if (!base::DeleteFile(tmp_split_dir)) {
    PLOG(ERROR) << "Failed removing " << tmp_split_dir.value();
  }

  // Move extensions in the keeplist back to Ash's profile directory.
  TaskResult task_result = MoveAshSubdirs(
      tmp_profile_dir, original_profile_dir,
      browser_data_migrator_util::kExtensionsFilePath,
      TaskStatus::kMoveSplitItemsToOriginalDirMoveExtensionsFailed);
  if (task_result.status != TaskStatus::kSucceeded)
    return task_result;

  // Move Storage objects related to extensions in the keeplist back to Ash's
  // profile directory.
  task_result = MoveAshSubdirs(
      tmp_profile_dir, original_profile_dir,
      base::FilePath(browser_data_migrator_util::kStorageFilePath)
          .Append(browser_data_migrator_util::kStorageExtFilePath)
          .value(),
      TaskStatus::kMoveSplitItemsToOriginalDirMoveStorageFailed);
  if (task_result.status != TaskStatus::kSucceeded)
    return task_result;

  // Move IndexedDB objects related to extensions in the keeplist back to Ash's
  // profile directory.
  const base::FilePath lacros_indexed_db_dir =
      tmp_profile_dir.Append(browser_data_migrator_util::kIndexedDBFilePath);
  if (base::PathExists(lacros_indexed_db_dir)) {
    const base::FilePath ash_indexed_db_dir = original_profile_dir.Append(
        browser_data_migrator_util::kIndexedDBFilePath);
    if (!base::CreateDirectory(ash_indexed_db_dir)) {
      PLOG(ERROR) << "CreateDirectory() failed for  "
                  << ash_indexed_db_dir.value();
      return {TaskStatus::kMoveSplitItemsToOriginalDirCreateDirFailed, errno};
    }

    for (const char* extension_id :
         browser_data_migrator_util::kExtensionsAshOnly) {
      if (!browser_data_migrator_util::MigrateAshIndexedDB(
              tmp_profile_dir, ash_indexed_db_dir, extension_id,
              /*copy=*/false)) {
        return {TaskStatus::kMoveSplitItemsToOriginalDirMoveIndexedDBFailed,
                errno};
      }
    }
  }

  return {TaskStatus::kSucceeded};
}

void MoveMigrator::OnMoveSplitItemsToOriginalDir(TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "Moving split objects has failed.";
    InvokeCallback(result);
    return;
  }

  SetResumeStep(local_state_, user_id_hash_, ResumeStep::kMoveTmpDir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&MoveMigrator::MoveTmpDirToLacrosDir,
                     original_profile_dir_),
      base::BindOnce(&MoveMigrator::OnMoveTmpDirToLacrosDir,
                     weak_factory_.GetWeakPtr()));
}

// static
MoveMigrator::TaskResult MoveMigrator::MoveTmpDirToLacrosDir(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running MoveTmpDirToLacrosDir()";

  // Move the newly created lacros user data dir into the final location where
  // lacros chrome can access.
  if (!base::Move(
          original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir),
          original_profile_dir.Append(
              browser_data_migrator_util::kLacrosDir))) {
    PLOG(ERROR) << "Failed moving "
                << original_profile_dir
                       .Append(browser_data_migrator_util::kMoveTmpDir)
                       .value()
                << " to "
                << original_profile_dir
                       .Append(browser_data_migrator_util::kLacrosDir)
                       .value();
    return {TaskStatus::kMoveTmpDirToLacrosDirMoveFailed, errno};
  }

  return {TaskStatus::kSucceeded};
}

void MoveMigrator::OnMoveTmpDirToLacrosDir(TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "Moving tmp dir to lacros dir failed.";
    InvokeCallback(result);
    return;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES(kMoveMigratorSuccessfulMigrationTimeUMA,
                             timer_.Elapsed());
  SetResumeStep(local_state_, user_id_hash_, ResumeStep::kCompleted);

  LOG(WARNING) << "Move migration completed successfully.";
  InvokeCallback(result);
}

// static
MoveMigrator::TaskResult MoveMigrator::CopyBothChromesSubdirs(
    const base::FilePath& original_profile_dir,
    const base::FilePath& tmp_split_dir,
    const std::string& target_dir,
    TaskStatus copy_fail_status) {
  const base::FilePath original_target_dir =
      original_profile_dir.Append(target_dir);

  if (base::PathExists(original_target_dir)) {
    const base::FilePath split_target_dir = tmp_split_dir.Append(target_dir);
    if (!base::CreateDirectory(split_target_dir)) {
      PLOG(ERROR) << "CreateDirectory() failed for  "
                  << split_target_dir.value();
      return {TaskStatus::kSetupAshDirCreateDirFailed, errno};
    }

    // Copy objects that belong to both Ash and Lacros.
    for (const char* extension_id :
         browser_data_migrator_util::kExtensionsBothChromes) {
      base::FilePath original_target_path =
          original_target_dir.Append(extension_id);

      if (base::PathExists(original_target_path)) {
        base::FilePath split_target_path =
            split_target_dir.Append(extension_id);

        if (!base::CopyDirectory(original_target_path, split_target_path,
                                 /*recursive=*/true)) {
          PLOG(ERROR) << "Failed copying " << original_target_path.value()
                      << " to " << split_target_path.value();
          return {copy_fail_status, errno};
        }
      }
    }
  }

  return {TaskStatus::kSucceeded};
}

// static
MoveMigrator::TaskResult MoveMigrator::MoveAshSubdirs(
    const base::FilePath& tmp_profile_dir,
    const base::FilePath& original_profile_dir,
    const std::string& target_dir,
    TaskStatus move_fail_status) {
  const base::FilePath lacros_target_dir = tmp_profile_dir.Append(target_dir);

  if (base::PathExists(lacros_target_dir)) {
    const base::FilePath ash_target_dir =
        original_profile_dir.Append(target_dir);
    if (!base::CreateDirectory(ash_target_dir)) {
      PLOG(ERROR) << "CreateDirectory() failed for  " << ash_target_dir.value();
      return {TaskStatus::kMoveSplitItemsToOriginalDirCreateDirFailed, errno};
    }

    // Move Ash-only objects.
    for (const char* extension_id :
         browser_data_migrator_util::kExtensionsAshOnly) {
      base::FilePath lacros_path = lacros_target_dir.Append(extension_id);
      if (base::PathExists(lacros_path)) {
        base::FilePath ash_path = ash_target_dir.Append(extension_id);
        if (!base::Move(lacros_path, ash_path)) {
          PLOG(ERROR) << "Failed moving " << lacros_path.value() << " to "
                      << ash_path.value();
          return {move_fail_status, errno};
        }
      }
    }
  }

  return {TaskStatus::kSucceeded};
}

void MoveMigrator::InvokeCallback(TaskResult result) {
  UMA_HISTOGRAM_ENUMERATION(kMoveMigratorTaskStatusUMA, result.status);
  if (result.status != TaskStatus::kSucceeded &&
      result.posix_errno.has_value()) {
    RecordPosixErrnoUMA(result.status, result.posix_errno.value());
  }

  std::move(finished_callback_)
      .Run(ToBrowserDataMigratorMigrationResult(result));
}

BrowserDataMigratorImpl::MigrationResult
MoveMigrator::ToBrowserDataMigratorMigrationResult(TaskResult result) {
  switch (result.status) {
    case TaskStatus::kSucceeded:
      return {BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigratorImpl::ResultKind::kSucceeded}};
    case TaskStatus::kCancelled:
      return {BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigratorImpl::ResultKind::kSkipped}};
    case TaskStatus::kPreMigrationCleanUpDeleteLacrosDirFailed:
    case TaskStatus::kPreMigrationCleanUpDeleteTmpDirFailed:
    case TaskStatus::kPreMigrationCleanUpDeleteTmpSplitDirFailed:
      return {BrowserDataMigratorImpl::DataWipeResult::kFailed,
              {BrowserDataMigratorImpl::ResultKind::kFailed}};
    case TaskStatus::kPreMigrationCleanUpNotEnoughSpace:
      return {BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigratorImpl::ResultKind::kFailed,
               result.extra_bytes_required_to_be_freed.value()}};
    case TaskStatus::kSetupLacrosDirCreateTmpDirFailed:
    case TaskStatus::kSetupLacrosDirCreateTmpProfileDirFailed:
    case TaskStatus::kSetupLacrosDirCopyTargetItemsFailed:
    case TaskStatus::kSetupLacrosDirWriteFirstRunSentinelFileFailed:
    case TaskStatus::kSetupAshDirCreateSplitDirFailed:
    case TaskStatus::kSetupAshDirMigrateLevelDBForLocalStateFailed:
    case TaskStatus::kSetupAshDirMigrateLevelDBForStateFailed:
    case TaskStatus::kSetupAshDirMigratePreferencesFailed:
    case TaskStatus::kMoveLacrosItemsToNewDirNoWritePerm:
    case TaskStatus::kMoveLacrosItemsToNewDirMoveFailed:
    case TaskStatus::kMoveSplitItemsToOriginalDirMoveSplitItemsFailed:
    case TaskStatus::kMoveSplitItemsToOriginalDirCreateDirFailed:
    case TaskStatus::kMoveSplitItemsToOriginalDirMoveExtensionsFailed:
    case TaskStatus::kMoveSplitItemsToOriginalDirMoveIndexedDBFailed:
    case TaskStatus::kMoveTmpDirToLacrosDirMoveFailed:
    case TaskStatus::kSetupAshDirCreateDirFailed:
    case TaskStatus::kSetupAshDirCopyExtensionsFailed:
    case TaskStatus::kSetupAshDirCopyIndexedDBFailed:
    case TaskStatus::kSetupAshDirMigrateSyncDataLevelDBFailed:
    case TaskStatus::kSetupAshDirCopyStorageFailed:
    case TaskStatus::kMoveSplitItemsToOriginalDirMoveStorageFailed:
    case TaskStatus::kMoveLacrosItemsCreateDirFailed:
      return {BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigratorImpl::ResultKind::kFailed}};
  }
}

// static
void MoveMigrator::RecordPosixErrnoUMA(TaskStatus task_status,
                                       const int posix_errno) {
  if (posix_errno == 0)
    return;

  std::string uma_name =
      kMoveMigratorPosixErrnoUMA + TaskStatusToString(task_status);
  base::UmaHistogramSparse(uma_name, posix_errno);
}

// static
std::string MoveMigrator::TaskStatusToString(TaskStatus task_status) {
  switch (task_status) {
#define MAPPING(name)       \
  case TaskStatus::k##name: \
    return #name
    MAPPING(Succeeded);
    MAPPING(Cancelled);
    MAPPING(PreMigrationCleanUpDeleteLacrosDirFailed);
    MAPPING(PreMigrationCleanUpDeleteTmpDirFailed);
    MAPPING(PreMigrationCleanUpDeleteTmpSplitDirFailed);
    MAPPING(PreMigrationCleanUpNotEnoughSpace);
    MAPPING(SetupLacrosDirCreateTmpDirFailed);
    MAPPING(SetupLacrosDirCreateTmpProfileDirFailed);
    MAPPING(SetupLacrosDirCopyTargetItemsFailed);
    MAPPING(SetupLacrosDirWriteFirstRunSentinelFileFailed);
    MAPPING(SetupAshDirCreateSplitDirFailed);
    MAPPING(SetupAshDirMigrateLevelDBForLocalStateFailed);
    MAPPING(SetupAshDirMigrateLevelDBForStateFailed);
    MAPPING(SetupAshDirMigratePreferencesFailed);
    MAPPING(MoveLacrosItemsToNewDirNoWritePerm);
    MAPPING(MoveLacrosItemsToNewDirMoveFailed);
    MAPPING(MoveSplitItemsToOriginalDirMoveSplitItemsFailed);
    MAPPING(MoveSplitItemsToOriginalDirCreateDirFailed);
    MAPPING(MoveSplitItemsToOriginalDirMoveExtensionsFailed);
    MAPPING(MoveSplitItemsToOriginalDirMoveIndexedDBFailed);
    MAPPING(MoveTmpDirToLacrosDirMoveFailed);
    MAPPING(SetupAshDirCreateDirFailed);
    MAPPING(SetupAshDirCopyExtensionsFailed);
    MAPPING(SetupAshDirCopyIndexedDBFailed);
    MAPPING(SetupAshDirMigrateSyncDataLevelDBFailed);
    MAPPING(SetupAshDirCopyStorageFailed);
    MAPPING(MoveSplitItemsToOriginalDirMoveStorageFailed);
    MAPPING(MoveLacrosItemsCreateDirFailed);
#undef MAPPING
  }
}

}  // namespace ash
