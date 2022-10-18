// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

#include <errno.h>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// Flag values for `switches::kForceBrowserDataBackwardMigrationForTesting`.
const char kBrowserDataBackwardMigrationForceSkip[] = "force-skip";
const char kBrowserDataBackwardMigrationForceMigration[] = "force-migration";

}  // namespace

BrowserDataBackMigrator::BrowserDataBackMigrator(
    const base::FilePath& ash_profile_dir)
    : ash_profile_dir_(ash_profile_dir) {}

BrowserDataBackMigrator::~BrowserDataBackMigrator() = default;

void BrowserDataBackMigrator::Migrate(
    BackMigrationFinishedCallback finished_callback) {
  LOG(WARNING) << "BrowserDataBackMigrator::Migrate() is called.";

  DCHECK(IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kBeforeInit));

  const base::FilePath lacros_profile_dir =
      ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::PreMigrationCleanUp,
                     ash_profile_dir_, lacros_profile_dir),
      base::BindOnce(&BrowserDataBackMigrator::OnPreMigrationCleanUp,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::PreMigrationCleanUp(
    const base::FilePath& ash_profile_dir,
    const base::FilePath& lacros_profile_dir) {
  LOG(WARNING) << "Running PreMigrationCleanUp()";

  const base::FilePath tmp_user_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);
  if (base::PathExists(tmp_user_dir)) {
    // Delete tmp_user_dir if any were left from a previous failed back
    // migration attempt.
    if (!base::DeletePathRecursively(tmp_user_dir)) {
      PLOG(ERROR) << "Deleting " << tmp_user_dir.value() << " failed: ";
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
      // This is not critical to the migration so log the error but do stop the
      // migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  // Delete lacros deletable items to free up space.
  browser_data_migrator_util::TargetItems lacros_deletable_items =
      browser_data_migrator_util::GetTargetItems(
          lacros_profile_dir, browser_data_migrator_util::ItemType::kDeletable);
  for (const auto& item : lacros_deletable_items.items) {
    bool result = item.is_directory ? base::DeletePathRecursively(item.path)
                                    : base::DeleteFile(item.path);
    if (!result) {
      // This is not critical to the migration so log the error, but do not stop
      // the migration.
      PLOG(ERROR) << "Could not delete " << item.path.value();
    }
  }

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnPreMigrationCleanUp(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "PreMigrationCleanup() failed.";
    std::move(finished_callback).Run(ToResult(result));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::MergeSplitItems,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnMergeSplitItems,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::MergeSplitItems(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running MergeSplitItems()";

  // TODO(b/244573664): Not yet implemented.

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnMergeSplitItems(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MergeSplitItems() failed.";
    std::move(finished_callback).Run(ToResult(result));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::MoveLacrosItemsBackToAsh,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnMoveLacrosItemsBackToAsh,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::MoveLacrosItemsBackToAsh(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running MoveLacrosItemsBackToAsh()";

  // TODO(b/244573664): Not yet implemented.

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnMoveLacrosItemsBackToAsh(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MoveLacrosItemsBackToAsh() failed.";
    std::move(finished_callback).Run(ToResult(result));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::MoveMergedItemsBackToAsh,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnMoveMergedItemsBackToAsh,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult
BrowserDataBackMigrator::MoveMergedItemsBackToAsh(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running MoveMergedItemsBackToAsh()";

  // TODO(b/244573664): Not yet implemented.

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnMoveMergedItemsBackToAsh(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "MoveMergedItemsBackToAsh() failed.";
    std::move(finished_callback).Run(ToResult(result));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::DeleteLacrosDir,
                     ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnDeleteLacrosDir,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::DeleteLacrosDir(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running DeleteLacrosDir()";

  const base::FilePath lacros_profile_dir =
      ash_profile_dir.Append(browser_data_migrator_util::kLacrosDir);

  if (base::PathExists(lacros_profile_dir)) {
    if (!base::DeletePathRecursively(lacros_profile_dir)) {
      PLOG(ERROR) << "Deleting " << lacros_profile_dir.value() << " failed: ";
      return {TaskStatus::kDeleteLacrosDirDeleteFailed, errno};
    }
  }

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnDeleteLacrosDir(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "DeleteLacrosDir() failed.";
    std::move(finished_callback).Run(ToResult(result));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataBackMigrator::DeleteTmpDir, ash_profile_dir_),
      base::BindOnce(&BrowserDataBackMigrator::OnDeleteTmpDir,
                     weak_factory_.GetWeakPtr(), std::move(finished_callback)));
}

// static
BrowserDataBackMigrator::TaskResult BrowserDataBackMigrator::DeleteTmpDir(
    const base::FilePath& ash_profile_dir) {
  LOG(WARNING) << "Running DeleteTmpDir()";

  const base::FilePath tmp_user_dir =
      ash_profile_dir.Append(browser_data_back_migrator::kTmpDir);
  if (base::PathExists(tmp_user_dir)) {
    if (!base::DeletePathRecursively(tmp_user_dir)) {
      PLOG(ERROR) << "Deleting " << tmp_user_dir.value() << " failed: ";
      return {TaskStatus::kDeleteTmpDirDeleteFailed, errno};
    }
  }

  return {TaskStatus::kSucceeded};
}

void BrowserDataBackMigrator::OnDeleteTmpDir(
    BackMigrationFinishedCallback finished_callback,
    BrowserDataBackMigrator::TaskResult result) {
  if (result.status != TaskStatus::kSucceeded) {
    LOG(ERROR) << "DeleteTmpDir() failed.";
    std::move(finished_callback).Run(ToResult(result));
    return;
  }

  LOG(WARNING) << "Backward migration completed successfully.";
  std::move(finished_callback).Run(ToResult(result));
}

// static
BrowserDataBackMigrator::Result BrowserDataBackMigrator::ToResult(
    TaskResult result) {
  switch (result.status) {
    case TaskStatus::kSucceeded:
      return Result::kSucceeded;
    case TaskStatus::kPreMigrationCleanUpDeleteTmpDirFailed:
    case TaskStatus::kDeleteLacrosDirDeleteFailed:
    case TaskStatus::kDeleteTmpDirDeleteFailed:
      return Result::kFailed;
  }
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
    crosapi::browser_util::PolicyInitState policy_init_state) {
  if (IsBackMigrationForceEnabled()) {
    return true;
  }

  // Check if migration should be force skipped.
  const std::string force_migration_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceBrowserDataBackwardMigration);

  if (force_migration_switch == kBrowserDataBackwardMigrationForceSkip) {
    return false;
  }

  if (policy_init_state ==
      crosapi::browser_util::PolicyInitState::kBeforeInit) {
    // TODO(b/244572632): Read cached flag.
  } else {
    DCHECK_EQ(policy_init_state,
              crosapi::browser_util::PolicyInitState::kAfterInit);
    // TODO(b/244572632): Read policy value.
  }

  return base::FeatureList::IsEnabled(
      ash::features::kLacrosProfileBackwardMigration);
}

// static
bool BrowserDataBackMigrator::ShouldMigrateBack(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
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

  const base::FilePath lacros_profile_dir =
      ash_data_dir.Append(browser_data_migrator_util::kLacrosDir);

  {
    // Temporarily allow blocking since we need to check if we need to migrate
    // the data from lacros to ash before the user has a chance to use it.
    base::ScopedAllowBlocking allow_blocking;

    // Synchronously check if the lacros folder is present.
    if (!DirectoryExists(lacros_profile_dir)) {
      VLOG(1) << "Lacros folder not found at '" << lacros_profile_dir.value()
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

  // TODO(b/253621578): Add g_attempt_restart helper for testing
  chrome::AttemptRestart();
  return true;
}

// static
bool BrowserDataBackMigrator::MaybeRestartToMigrateBack(
    const AccountId& account_id,
    const std::string& user_id_hash,
    crosapi::browser_util::PolicyInitState policy_init_state) {
  if (!ShouldMigrateBack(account_id, user_id_hash, policy_init_state)) {
    return false;
  }

  return RestartToMigrateBack(account_id);
}

}  // namespace ash
