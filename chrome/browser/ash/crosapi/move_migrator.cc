// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/move_migrator.h"

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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
    if (resume_count > kMoveMigrationResumeCountLimit) {
      LOG(ERROR) << "The number of resume attempt limit has reached. Marking "
                    "move migration as completed.";
      SetResumeStep(local_state_, user_id_hash_, ResumeStep::kCompleted);
      resume_step = ResumeStep::kCompleted;
    }
  }

  // Start or resume migration.
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
    case ResumeStep::kMoveTmpDir:
      LOG(ERROR) << "Migration did not complete in the previous attempt. "
                    "Resuming migration from kMoveDir step.";
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
      std::move(finished_callback_)
          .Run({BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
                {BrowserDataMigrator::ResultKind::kSucceeded}});
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
    case ResumeStep::kMoveTmpDir:
      return true;
    case ResumeStep::kCompleted:
      return false;
  }
}

// static
void MoveMigrator::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMoveMigrationResumeStepPref,
                                   base::DictionaryValue());
  registry->RegisterDictionaryPref(kMoveMigrationResumeCountPref,
                                   base::DictionaryValue());
}

// static
MoveMigrator::ResumeStep MoveMigrator::GetResumeStep(
    PrefService* local_state,
    const std::string& user_id_hash) {
  return static_cast<ResumeStep>(
      local_state->GetDictionary(kMoveMigrationResumeStepPref)
          ->FindIntKey(user_id_hash)
          .value_or(0));
}

// static
void MoveMigrator::SetResumeStep(PrefService* local_state,
                                 const std::string& user_id_hash,
                                 const ResumeStep step) {
  DictionaryPrefUpdate update(local_state, kMoveMigrationResumeStepPref);
  base::Value* dict = update.Get();
  dict->SetKey(user_id_hash, base::Value(static_cast<int>(step)));
  local_state->CommitPendingWrite();
}

int MoveMigrator::UpdateResumeAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  int count = local_state->GetDictionary(kMoveMigrationResumeCountPref)
                  ->FindIntPath(user_id_hash)
                  .value_or(0);
  count += 1;
  DictionaryPrefUpdate update(local_state, kMoveMigrationResumeCountPref);
  base::Value* dict = update.Get();
  dict->SetIntKey(user_id_hash, count);
  return count;
}

void MoveMigrator::ClearResumeAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  DictionaryPrefUpdate update(local_state, kMoveMigrationResumeCountPref);
  base::Value* dict = update.Get();
  dict->RemoveKey(user_id_hash);
}

// static
void MoveMigrator::ClearResumeStepForUser(PrefService* local_state,
                                          const std::string& user_id_hash) {
  DictionaryPrefUpdate update(local_state, kMoveMigrationResumeStepPref);
  base::Value* dict = update.Get();
  dict->RemoveKey(user_id_hash);
}

// static
MoveMigrator::PreMigrationCleanUpResult MoveMigrator::PreMigrationCleanUp(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running PreMigrationCleanUp()";

  const base::FilePath new_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  if (base::PathExists(new_user_dir)) {
    // Delete an existing lacros profile before the migration.
    if (!base::DeletePathRecursively(new_user_dir)) {
      PLOG(ERROR) << "Deleting " << new_user_dir.value() << " failed: ";
      return {false};
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
      return {false};
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
          browser_data_migrator_util::ItemType::kNeedCopy);

  const int64_t extra_bytes_required_to_be_freed =
      browser_data_migrator_util::ExtraBytesRequiredToBeFreed(
          need_copy_items.total_size, original_profile_dir);

  return {true, extra_bytes_required_to_be_freed};
}

void MoveMigrator::OnPreMigrationCleanUp(
    MoveMigrator::PreMigrationCleanUpResult result) {
  if (!result.success) {
    LOG(ERROR) << "PreMigrationCleanup() failed.";
    std::move(finished_callback_)
        .Run({BrowserDataMigratorImpl::DataWipeResult::kFailed,
              {BrowserDataMigrator::ResultKind::kFailed}});
    return;
  }

  if (result.extra_bytes_required_to_be_freed.value() > 0u) {
    LOG(ERROR) << "Not enough disk space available to carry out the migration "
                  "safely. Need to free up "
               << result.extra_bytes_required_to_be_freed.value()
               << " bytes from " << original_profile_dir_.value();
    std::move(finished_callback_)
        .Run({BrowserDataMigratorImpl::DataWipeResult::kFailed,
              {BrowserDataMigratorImpl::ResultKind::kFailed,
               result.extra_bytes_required_to_be_freed}});
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
bool MoveMigrator::SetupLacrosDir(
    const base::FilePath& original_profile_dir,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag) {
  LOG(WARNING) << "Running SetupLacrosDir()";

  if (cancel_flag->IsSet()) {
    LOG(WARNING) << "Migration is cancelled.";
    return false;
  }
  const base::FilePath tmp_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir);
  const base::FilePath tmp_profile_dir =
      tmp_user_dir.Append(browser_data_migrator_util::kLacrosProfilePath);

  if (!base::CreateDirectory(tmp_user_dir)) {
    PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_user_dir.value();
    return false;
  }
  if (!base::CreateDirectory(tmp_profile_dir)) {
    PLOG(ERROR) << "CreateDirectory() failed for  " << tmp_profile_dir.value();
    return false;
  }

  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir,
          browser_data_migrator_util::ItemType::kNeedCopy);

  progress_tracker->SetTotalSizeToCopy(need_copy_items.total_size);

  if (!browser_data_migrator_util::CopyTargetItems(
          tmp_profile_dir, need_copy_items, cancel_flag.get(),
          progress_tracker.get())) {
    LOG(ERROR) << "CopyTargetItems() failed for need_copy_items.";
    return false;
  }

  if (!base::WriteFile(tmp_user_dir.Append(chrome::kFirstRunSentinel), "")) {
    LOG(ERROR) << "WriteFile() failed for " << chrome::kFirstRunSentinel;
    return false;
  }

  return true;
}

void MoveMigrator::OnSetupLacrosDir(bool success) {
  if (!success) {
    LOG(ERROR) << "MoveMigrator::SetupLacrosDir() failed.";
    std::move(finished_callback_)
        .Run({BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigrator::ResultKind::kFailed}});
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
bool MoveMigrator::MoveLacrosItemsToNewDir(
    const base::FilePath& original_profile_dir) {
  LOG(WARNING) << "Running MoveLacrosItemsToNewDir()";

  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir, browser_data_migrator_util::ItemType::kLacros);

  for (const auto& item : lacros_items.items) {
    if (item.is_directory && !base::PathIsWritable(item.path)) {
      // TODO(ythjkt): Add a UMA.
      PLOG(ERROR) << "The current process does not have write permission to "
                     "the directory "
                  << item.path.value();
      return false;
    }
  }

  const base::FilePath tmp_profile_dir =
      original_profile_dir.Append(browser_data_migrator_util::kMoveTmpDir)
          .Append(browser_data_migrator_util::kLacrosProfilePath);

  for (const auto& item : lacros_items.items) {
    if (!base::Move(item.path, tmp_profile_dir.Append(item.path.BaseName()))) {
      PLOG(ERROR) << "Failed to move item " << item.path.value() << " to "
                  << tmp_profile_dir.Append(item.path.BaseName()) << ": ";
      return false;
    }
  }
  return true;
}

void MoveMigrator::OnMoveLacrosItemsToNewDir(bool success) {
  if (!success) {
    LOG(ERROR) << "Moving Lacros items to temporary directory failed.";
    std::move(finished_callback_)
        .Run({BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigrator::ResultKind::kFailed}});
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
bool MoveMigrator::MoveTmpDirToLacrosDir(
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
    return false;
  }

  return true;
}

void MoveMigrator::OnMoveTmpDirToLacrosDir(bool success) {
  if (!success) {
    LOG(ERROR) << "Moving tmp dir to lacros dir failed.";
    std::move(finished_callback_)
        .Run({BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
              {BrowserDataMigrator::ResultKind::kFailed}});
    return;
  }

  SetResumeStep(local_state_, user_id_hash_, ResumeStep::kCompleted);
  LOG(WARNING) << "Move migration completed successfully.";
  std::move(finished_callback_)
      .Run({BrowserDataMigratorImpl::DataWipeResult::kSucceeded,
            {BrowserDataMigratorImpl::ResultKind::kSucceeded}});
}

}  // namespace ash
