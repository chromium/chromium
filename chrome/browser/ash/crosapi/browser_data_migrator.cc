// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/constants/ash_switches.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"

namespace ash {
namespace {
// Flag values for `switches::kForceBrowserDataMigrationForTesting`.
const char kBrowserDataMigrationForceSkip[] = "force-skip";
const char kBrowserDataMigrationForceMigration[] = "force-migration";
}  // namespace

// static
bool BrowserDataMigratorImpl::MaybeRestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove this
  // log message.
  LOG(WARNING) << "MaybeRestartToMigrate() is called.";
  // If `MigrationStep` is not `kCheckStep`, `MaybeRestartToMigrate()` has
  // already moved on to later steps. Namely either in the middle of migration
  // or migration has already run.
  MigrationStep step = GetMigrationStep(g_browser_process->local_state());
  if (step != MigrationStep::kCheckStep) {
    switch (step) {
      case MigrationStep::kRestartCalled:
        LOG(ERROR) << "RestartToMigrate() was called but Migrate() was not. "
                      "This indicates that eitehr "
                      "SessionManagerClient::RequestBrowserDataMigration() "
                      "failed or ash crashed before reaching Migrate(). Check "
                      "the previous chrome log and the one before.";
        break;
      case MigrationStep::kStarted:
        LOG(ERROR) << "Migrate() was called but "
                      "MigrateInternalFinishedUIThread() was not indicating "
                      "that ash might have crashed during the migration.";
        break;
      case MigrationStep::kEnded:
      default:
        // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises,
        // remove
        // this log message or reduce to VLOG(1).
        LOG(WARNING)
            << "Migration has ended and either completed or failed. step = "
            << static_cast<int>(step);
        break;
    }

    return false;
  }

  // Check if the switch for testing is present.
  const std::string force_migration_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceBrowserDataMigrationForTesting);
  if (force_migration_switch == kBrowserDataMigrationForceSkip)
    return false;
  if (force_migration_switch == kBrowserDataMigrationForceMigration) {
    LOG(WARNING) << "`kBrowserDataMigrationForceMigration` switch is present.";
    return RestartToMigrate(account_id, user_id_hash);
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // Check if user exists i.e. not a guest session.
  if (!user)
    return false;
  // Check if lacros is enabled. If not immediately return.
  if (!crosapi::browser_util::IsLacrosEnabledForMigration(user,
                                                          policy_init_state)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING)
        << "Lacros is disabled. Call ClearMigrationAttemptCountForUser() so "
           "that the migration can be attempted again after once lacros is "
           "enabled again.";

    // If lacros is not enabled other than reaching the maximum retry count of
    // profile migration, clear the retry count and
    // `kProfileMigrationCompletedForUserPref`. This will allow users to retry
    // profile migration after disabling and re-enabling lacros.
    ClearMigrationAttemptCountForUser(g_browser_process->local_state(),
                                      user_id_hash);
    crosapi::browser_util::ClearProfileMigrationCompletedForUser(
        g_browser_process->local_state(), user_id_hash);
    return false;
  }

  //  Currently we turn on profile migration only for Googlers. To test profile
  //  migration without @google.com account, enable feature
  //  `kLacrosProfileMigrationForAnyUser` defined in browser_util.
  // TODO(crbug.com/1266669): Remove this check once profile migration is
  // enabled for all users.
  if (!crosapi::browser_util::IsProfileMigrationEnabled(account_id)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Profile migration is disabled.";
    return false;
  }

  // If the user is a new user, then there shouldn't be anything to migrate.
  // Also mark the user as migration completed.
  if (user_manager::UserManager::Get()->IsCurrentUserNew()) {
    crosapi::browser_util::RecordDataVer(g_browser_process->local_state(),
                                         user_id_hash,
                                         version_info::GetVersion());

    crosapi::browser_util::SetProfileMigrationCompletedForUser(
        g_browser_process->local_state(), user_id_hash);
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Setting migration as completed since it is a new user.";
    return false;
  }

  int attempts = GetMigrationAttemptCountForUser(
      g_browser_process->local_state(), user_id_hash);
  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING) << "Attempt #" << attempts;
  if (attempts >= kMaxMigrationAttemptCount) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING) << "Skipping profile migration since migration attemp count = "
                 << attempts << " has exceeded " << kMaxMigrationAttemptCount;
    return false;
  }

  if (crosapi::browser_util::IsDataWipeRequired(user_id_hash)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING)
        << "Restarting to run profile migration since data wipe is required.";
    // If data wipe is required, no need for a further check to determine if
    // lacros data dir exists or not.
    return RestartToMigrate(account_id, user_id_hash);
  }

  if (crosapi::browser_util::IsProfileMigrationCompletedForUser(
          g_browser_process->local_state(), user_id_hash)) {
    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises,
    // remove this log message.
    LOG(WARNING) << "Profile migration has been completed already.";
    return false;
  }

  return RestartToMigrate(account_id, user_id_hash);
}

// static
bool BrowserDataMigratorImpl::RestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash) {
  SetMigrationStep(g_browser_process->local_state(),
                   MigrationStep::kRestartCalled);

  UpdateMigrationAttemptCountForUser(g_browser_process->local_state(),
                                     user_id_hash);

  crosapi::browser_util::ClearProfileMigrationCompletedForUser(
      g_browser_process->local_state(), user_id_hash);

  g_browser_process->local_state()->CommitPendingWrite();

  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
  // this log message.
  LOG(WARNING) << "Making a dbus method call to session_manager";
  bool success = SessionManagerClient::Get()->RequestBrowserDataMigration(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id));

  if (!success)
    return false;

  chrome::AttemptRestart();
  return true;
}

BrowserDataMigratorImpl::BrowserDataMigratorImpl(
    const base::FilePath& original_profile_dir,
    const std::string& user_id_hash,
    const ProgressCallback& progress_callback,
    base::OnceClosure completion_callback,
    PrefService* local_state)
    : original_profile_dir_(original_profile_dir),
      user_id_hash_(user_id_hash),
      progress_tracker_(
          std::make_unique<MigrationProgressTrackerImpl>(progress_callback)),
      completion_callback_(std::move(completion_callback)),
      cancel_flag_(
          base::MakeRefCounted<browser_data_migrator_util::CancelFlag>()),
      local_state_(local_state),
      final_status_(ResultValue::kSkipped) {
  DCHECK(local_state_);
}

BrowserDataMigratorImpl::~BrowserDataMigratorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserDataMigratorImpl::Migrate() {
  DCHECK(local_state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING) << "BrowserDataMigratorImpl::Migrate() is called.";

  DCHECK(GetMigrationStep(local_state_) == MigrationStep::kRestartCalled);
  SetMigrationStep(local_state_, MigrationStep::kStarted);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataMigratorImpl::MigrateInternal,
                     original_profile_dir_, std::move(progress_tracker_),
                     cancel_flag_),
      base::BindOnce(&BrowserDataMigratorImpl::MigrateInternalFinishedUIThread,
                     weak_factory_.GetWeakPtr()));
}

void BrowserDataMigratorImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancel_flag_->Set();
}

// static
// TODO(crbug.com/1178702): Once testing phase is over and lacros becomes the
// only web browser, update the underlying logic of migration from copy to move.
// Note that during testing phase we are copying files and leaving files in
// original location intact. We will allow these two states to diverge.
BrowserDataMigratorImpl::MigrationResult
BrowserDataMigratorImpl::MigrateInternal(
    const base::FilePath& original_profile_dir,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<browser_data_migrator_util::CancelFlag> cancel_flag) {
  ResultValue data_wipe_result = ResultValue::kSkipped;

  const base::FilePath tmp_dir =
      original_profile_dir.Append(browser_data_migrator_util::kTmpDir);
  const base::FilePath new_user_dir =
      original_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  if (base::DirectoryExists(new_user_dir)) {
    if (!base::DeletePathRecursively(new_user_dir)) {
      PLOG(ERROR) << "Deleting " << new_user_dir.value() << " failed: ";
      UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kDataWipeFailed);
      return {ResultValue::kFailed, ResultValue::kFailed};
    }
    data_wipe_result = ResultValue::kSucceeded;
  }

  // Check if tmp directory already exists and delete if it does.
  if (base::PathExists(tmp_dir)) {
    LOG(WARNING) << browser_data_migrator_util::kTmpDir
                 << " already exists indicating migration was aborted on the"
                    "previous attempt.";
    if (!base::DeletePathRecursively(tmp_dir)) {
      PLOG(ERROR) << "Failed to delete tmp dir";
      UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kDeleteTmpDirFailed);
      return {data_wipe_result, ResultValue::kFailed};
    }
  }

  browser_data_migrator_util::TargetItems need_copy_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir,
          browser_data_migrator_util::ItemType::kNeedCopy);
  browser_data_migrator_util::TargetItems lacros_items =
      browser_data_migrator_util::GetTargetItems(
          original_profile_dir, browser_data_migrator_util::ItemType::kLacros);
  const int64_t total_copy_size =
      need_copy_items.total_size + lacros_items.total_size;
  progress_tracker->SetTotalSizeToCopy(total_copy_size);

  base::ElapsedTimer timer;

  if (!browser_data_migrator_util::HasEnoughDiskSpace(total_copy_size,
                                                      original_profile_dir)) {
    UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kNotEnoughSpace);
    return {data_wipe_result, ResultValue::kFailed};
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
      return {data_wipe_result, ResultValue::kCancelled};
    }

    UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kCopyFailed);
    return {data_wipe_result, ResultValue::kFailed};
  }

  // Move `tmp_dir` to `new_user_dir`.
  if (!base::Move(tmp_dir, new_user_dir)) {
    PLOG(ERROR) << "Move failed";
    if (base::PathExists(tmp_dir)) {
      base::DeletePathRecursively(tmp_dir);
    }
    UMA_HISTOGRAM_ENUMERATION(kFinalStatus, FinalStatus::kMoveFailed);
    return {data_wipe_result, ResultValue::kFailed};
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
  return {data_wipe_result, ResultValue::kSucceeded};
}

void BrowserDataMigratorImpl::MigrateInternalFinishedUIThread(
    MigrationResult result) {
  DCHECK(local_state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetMigrationStep(local_state_) == MigrationStep::kStarted);
  SetMigrationStep(local_state_, MigrationStep::kEnded);

  final_status_ = result.data_migration;

  if (result.data_wipe != ResultValue::kFailed) {
    // kSkipped means that the directory did not exist so record the current
    // version as the data version.
    crosapi::browser_util::RecordDataVer(local_state_, user_id_hash_,
                                         version_info::GetVersion());
  }

  if (result.data_migration == ResultValue::kSucceeded) {
    crosapi::browser_util::SetProfileMigrationCompletedForUser(local_state_,
                                                               user_id_hash_);

    ClearMigrationAttemptCountForUser(local_state_, user_id_hash_);
  }
  // If migration has failed or skipped, we silently relaunch ash and send them
  // to their home screen. In that case lacros will be disabled.

  local_state_->CommitPendingWrite();

  std::move(completion_callback_).Run();
}

BrowserDataMigratorImpl::ResultValue BrowserDataMigratorImpl::GetFinalStatus() {
  return final_status_;
}

// static
bool BrowserDataMigratorImpl::SetupTmpDir(
    const browser_data_migrator_util::TargetItems& lacros_items,
    const browser_data_migrator_util::TargetItems& need_copy_items,
    const base::FilePath& tmp_dir,
    browser_data_migrator_util::CancelFlag* cancel_flag,
    MigrationProgressTracker* progress_tracker) {
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

// static
void BrowserDataMigratorImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kMigrationStep,
                                static_cast<int>(MigrationStep::kCheckStep));
  registry->RegisterDictionaryPref(kMigrationAttemptCountPref,
                                   base::DictionaryValue());
}

// static
void BrowserDataMigratorImpl::SetMigrationStep(
    PrefService* local_state,
    BrowserDataMigratorImpl::MigrationStep step) {
  local_state->SetInteger(kMigrationStep, static_cast<int>(step));
}

// static
void BrowserDataMigratorImpl::ClearMigrationStep(PrefService* local_state) {
  local_state->ClearPref(kMigrationStep);
}

// static
BrowserDataMigratorImpl::MigrationStep
BrowserDataMigratorImpl::GetMigrationStep(PrefService* local_state) {
  return static_cast<MigrationStep>(local_state->GetInteger(kMigrationStep));
}

// static
void BrowserDataMigratorImpl::UpdateMigrationAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  int count = GetMigrationAttemptCountForUser(local_state, user_id_hash);
  count += 1;
  DictionaryPrefUpdate update(local_state, kMigrationAttemptCountPref);
  base::Value* dict = update.Get();
  dict->SetIntKey(user_id_hash, count);
}

// static
int BrowserDataMigratorImpl::GetMigrationAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  return local_state->GetDictionary(kMigrationAttemptCountPref)
      ->FindIntPath(user_id_hash)
      .value_or(0);
}

// static
void BrowserDataMigratorImpl::ClearMigrationAttemptCountForUser(
    PrefService* local_state,
    const std::string& user_id_hash) {
  DictionaryPrefUpdate update(local_state, kMigrationAttemptCountPref);
  base::Value* dict = update.Get();
  dict->RemoveKey(user_id_hash);
}

}  // namespace ash
