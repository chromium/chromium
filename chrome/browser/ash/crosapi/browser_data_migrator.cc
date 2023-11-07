// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/move_migrator.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
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

base::RepeatingClosure* g_attempt_restart = nullptr;

// Checks if the disk space is enough to run profile migration.
// Returns the bytes required to be freed. Specifically, on success
// returns 0.
uint64_t DiskCheck(const base::FilePath& profile_data_dir) {
  using browser_data_migrator_util::GetTargetItems;
  using browser_data_migrator_util::ItemType;
  using browser_data_migrator_util::TargetItems;
  TargetItems deletable_items =
      GetTargetItems(profile_data_dir, ItemType::kDeletable);

  const int64_t required_size =
      browser_data_migrator_util::EstimatedExtraBytesCreated(profile_data_dir) -
      deletable_items.total_size;

  return browser_data_migrator_util::ExtraBytesRequiredToBeFreed(
      required_size, profile_data_dir);
}

}  // namespace

ScopedRestartAttemptForTesting::ScopedRestartAttemptForTesting(
    base::RepeatingClosure callback) {
  DCHECK(!g_attempt_restart);
  g_attempt_restart = new base::RepeatingClosure(std::move(callback));
}

ScopedRestartAttemptForTesting::~ScopedRestartAttemptForTesting() {
  DCHECK(g_attempt_restart);
  delete g_attempt_restart;
  g_attempt_restart = nullptr;
}

bool BrowserDataMigratorImpl::MaybeForceResumeMoveMigration(
    PrefService* local_state,
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  if (!MoveMigrator::ResumeRequired(local_state, user_id_hash))
    return false;

  LOG(WARNING) << "Calling RestartToMigrate() to resume move migration.";
  return RestartToMigrate(account_id, user_id_hash, local_state,
                          policy_init_state);
}

// static
void BrowserDataMigratorImpl::AttemptRestart() {
  if (g_attempt_restart) {
    g_attempt_restart->Run();
    return;
  }

  chrome::AttemptRestart();
}

// static
bool BrowserDataMigratorImpl::MaybeRestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  if (!MaybeRestartToMigrateInternal(account_id, user_id_hash,
                                     policy_init_state)) {
    return false;
  }
  return RestartToMigrate(account_id, user_id_hash,
                          user_manager::UserManager::Get()->GetLocalState(),
                          policy_init_state);
}

void BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheck(
    const AccountId& account_id,
    const std::string& user_id_hash,
    base::OnceCallback<void(bool, const absl::optional<uint64_t>&)> callback) {
  if (!MaybeRestartToMigrateInternal(
          account_id, user_id_hash,
          crosapi::browser_util::PolicyInitState::kAfterInit)) {
    std::move(callback).Run(false, absl::nullopt);
    return;
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(DFATAL) << "Could not get the original user data dir path.";
    std::move(callback).Run(false, absl::nullopt);
    return;
  }

  const base::FilePath profile_data_dir =
      user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&DiskCheck, profile_data_dir),
      base::BindOnce(&BrowserDataMigratorImpl::
                         MaybeRestartToMigrateWithDiskCheckAfterDiskCheck,
                     account_id, user_id_hash, std::move(callback)));
}

void BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheckAfterDiskCheck(
    const AccountId& account_id,
    const std::string& user_id_hash,
    base::OnceCallback<void(bool, const absl::optional<uint64_t>&)> callback,
    uint64_t required_size) {
  if (required_size > 0) {
    LOG(ERROR) << "Failed due to out of disk: " << required_size;
    std::move(callback).Run(false, required_size);
    return;
  }

  bool result =
      RestartToMigrate(account_id, user_id_hash,
                       user_manager::UserManager::Get()->GetLocalState(),
                       crosapi::browser_util::PolicyInitState::kAfterInit);
  std::move(callback).Run(result, absl::nullopt);
}

bool BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  auto* user_manager = user_manager::UserManager::Get();
  auto* local_state = user_manager->GetLocalState();

  // If `MigrationStep` is not `kCheckStep`, `MaybeRestartToMigrate()` has
  // already moved on to later steps. Namely either in the middle of migration
  // or migration has already run.
  MigrationStep step = GetMigrationStep(local_state);
  if (step != MigrationStep::kCheckStep) {
    switch (step) {
      case MigrationStep::kRestartCalled:
        LOG(ERROR)
            << "RestartToMigrate() was called but Migrate() was not. "
               "This indicates that either "
               "SessionManagerClient::BlockingRequestBrowserDataMigration() "
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
        // remove this log message or reduce to VLOG(1).
        if (ash::standalone_browser::migrator_util::
                IsProfileMigrationCompletedForUser(local_state, user_id_hash,
                                                   true /* print_mode */)) {
          LOG(WARNING) << "Migration was attempted and successfully completed.";
        } else {
          LOG(WARNING) << "Migration was attempted but failed or was skipped.";
        }
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
    return true;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // Check if user exists i.e. not a guest session.
  if (!user)
    return false;

  // Migration should not run for secondary users.
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  // `MaybeRestartToMigrateInternal()` either gets called before profile
  // initialization or after profile initialization. In case of the former, its
  // called from `PreProfileInit()` and this is only called for the primary
  // profile so we can assume that the user is the primary user if `primary_user
  // == nullptr`. If primary_user is not null then we check if `user !=
  // primary_user`.
  if (primary_user && (user != primary_user)) {
    LOG(WARNING) << "Skip migration for secondary users.";
    return false;
  }

  // Check if profile migration is enabled. If not immediately return.
  if (!crosapi::browser_util::IsProfileMigrationEnabled(user,
                                                        policy_init_state)) {
    if (crosapi::browser_util::IsLacrosEnabledForMigration(user,
                                                           policy_init_state) ||
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kSafeMode)) {
      // Skip clearing prefs if Lacros is enabled or Lacros is disabled due to
      // safe mode. Profile migration can be disabled even if Lacros is enabled
      // by enabling LacrosProfileMigrationForceOff flag. There's another case
      // where Lacros is disabled due to "safe mode" being enabled after Ash
      // crashes. By not clearing prefs in safe mode, we avoid the following
      // scenario: Ash experiences a crash loop due to some experimental flag ->
      // experimental flags get dropped including ones to enable Lacros ->
      // Lacros is disabled and migration completion flags gets cleared -> on
      // next login migration is run and wipes existing user data.
      LOG(WARNING)
          << "Profile migration is disabled but either Lacros is enabled or "
             "safe mode is enabled so skipping clearing prefs.";
      return false;
    }

    // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
    // this log message.
    LOG(WARNING)
        << "Lacros is disabled. Call ClearMigrationAttemptCountForUser() so "
           "that the migration can be attempted again once migration is "
           "enabled again.";

    // If Lacros is disabled clear the retry count and
    // `kProfileMigrationCompletedForUserPref` so that users may retry profile
    // migration when re-enabling Lacros.
    ash::standalone_browser::migrator_util::ClearMigrationAttemptCountForUser(
        local_state, user_id_hash);
    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(local_state, user_id_hash);
    MoveMigrator::ClearResumeStepForUser(local_state, user_id_hash);
    MoveMigrator::ClearResumeAttemptCountForUser(local_state, user_id_hash);
    return false;
  }

  if (ash::standalone_browser::migrator_util::
          IsMigrationAttemptLimitReachedForUser(local_state, user_id_hash)) {
    LOG(ERROR) << "Skipping profile migration since maximum migration "
                  "attempt count has been reached.";
    return false;
  }

  if (ash::standalone_browser::migrator_util::
          IsProfileMigrationCompletedForUser(local_state, user_id_hash,
                                             true /* print_mode */)) {
    LOG(WARNING) << "Profile migration is already completed at version "
                 << crosapi::browser_util::GetDataVer(local_state, user_id_hash)
                        .GetString();

    return false;
  }

  return true;
}

// static
bool BrowserDataMigratorImpl::RestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash,
    PrefService* local_state,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  SetMigrationStep(local_state, MigrationStep::kRestartCalled);

  ash::standalone_browser::migrator_util::UpdateMigrationAttemptCountForUser(
      local_state, user_id_hash);

  ash::standalone_browser::migrator_util::ClearProfileMigrationCompletedForUser(
      local_state, user_id_hash);
  crosapi::browser_util::ClearProfileMigrationCompletionTimeForUser(
      local_state, user_id_hash);

  local_state->CommitPendingWrite();

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // `user` should exist by the time `RestartToMigrate()` is called.
  CHECK(user) << "User could not be found for " << account_id.GetUserEmail()
              << " but RestartToMigrate() was called.";

  // TODO(crbug.com/1277848): Once `BrowserDataMigrator` stabilises, remove
  // this log message.
  LOG(WARNING) << "Making a dbus method call to session_manager";
  bool success =
      SessionManagerClient::Get()->BlockingRequestBrowserDataMigration(
          cryptohome::CreateAccountIdentifierFromAccountId(account_id),
          browser_data_migrator_util::kMoveSwitchValue);

  // TODO(crbug.com/1261730): Add an UMA.
  if (!success) {
    LOG(ERROR) << "SessionManagerClient::BlockingRequestBrowserDataMigration() "
                  "failed.";
    return false;
  }

  AttemptRestart();
  return true;
}

BrowserDataMigratorImpl::BrowserDataMigratorImpl(
    const base::FilePath& original_profile_dir,
    const std::string& user_id_hash,
    const ProgressCallback& progress_callback,
    PrefService* local_state)
    : original_profile_dir_(original_profile_dir),
      user_id_hash_(user_id_hash),
      progress_tracker_(
          std::make_unique<MigrationProgressTrackerImpl>(progress_callback)),
      cancel_flag_(
          base::MakeRefCounted<browser_data_migrator_util::CancelFlag>()),
      local_state_(local_state) {
  DCHECK(local_state_);
}

BrowserDataMigratorImpl::~BrowserDataMigratorImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserDataMigratorImpl::Migrate(MigrateCallback callback) {
  DCHECK(local_state_);
  DCHECK(completion_callback_.is_null());
  completion_callback_ = std::move(callback);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING) << "BrowserDataMigratorImpl::Migrate() is called.";

  DCHECK(GetMigrationStep(local_state_) == MigrationStep::kRestartCalled);
  SetMigrationStep(local_state_, MigrationStep::kStarted);

  LOG(WARNING) << "Initializing MoveMigrator.";
  migrator_delegate_ = std::make_unique<MoveMigrator>(
      original_profile_dir_, user_id_hash_, std::move(progress_tracker_),
      cancel_flag_, local_state_,
      base::BindOnce(&BrowserDataMigratorImpl::MigrateInternalFinishedUIThread,
                     weak_factory_.GetWeakPtr()));

  migrator_delegate_->Migrate();
}

void BrowserDataMigratorImpl::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancel_flag_->Set();
}

void BrowserDataMigratorImpl::MigrateInternalFinishedUIThread(

    MigrationResult result) {
  DCHECK(local_state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetMigrationStep(local_state_) == MigrationStep::kStarted);
  SetMigrationStep(local_state_, MigrationStep::kEnded);

  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING)
      << "MigrateInternalFinishedUIThread() called with results data wipe = "
      << static_cast<int>(result.data_wipe_result) << " and migration "
      << static_cast<int>(result.data_migration_result.kind);

  if (result.data_wipe_result != DataWipeResult::kFailed) {
    // kSkipped means that the directory did not exist so record the current
    // version as the data version.
    crosapi::browser_util::RecordDataVer(local_state_, user_id_hash_,
                                         version_info::GetVersion());
  }

  switch (result.data_migration_result.kind) {
    case ResultKind::kSucceeded:
      ash::standalone_browser::migrator_util::
          SetProfileMigrationCompletedForUser(
              local_state_, user_id_hash_,
              ash::standalone_browser::migrator_util::MigrationMode::kMove);

      // Profile migration is marked as completed both when the migration is
      // performed (here) and for a new user without actually performing data
      // migration (`ProfileImpl::OnLocaleReady`). The timestamp of completed
      // migration is only recorded when the migration is actually performed.
      crosapi::browser_util::SetProfileMigrationCompletionTimeForUser(
          local_state_, user_id_hash_);

      ash::standalone_browser::migrator_util::ClearMigrationAttemptCountForUser(
          local_state_, user_id_hash_);
      break;
    case ResultKind::kFailed:
      LOG(ERROR) << "Migration failed for some reason. Look at logs from "
                    "move_migrator.cc for details.";
      // This should not happen often. Send a crash report for debugging.
      base::debug::DumpWithoutCrashing();
      break;
    case ResultKind::kCancelled:
      LOG(WARNING) << "Migration was cancelled by the user.";
      break;
  }

  local_state_->CommitPendingWrite();

  std::move(completion_callback_).Run(result.data_migration_result);
}

// static
void BrowserDataMigratorImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kMigrationStep,
                                static_cast<int>(MigrationStep::kCheckStep));
  // Register prefs for move migration.
  MoveMigrator::RegisterLocalStatePrefs(registry);
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

}  // namespace ash
