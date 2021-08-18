// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {
// The name of temporary directory that will store copies of files from the
// original user data directory. At the end of the migration, it will be moved
// to the appropriate destination.
constexpr char kTmpDir[] = "browser_data_migrator";
// The base names of files and directories directly under the original profile
// data directory that should not be copied. e.g. caches or files only needed by
// ash.
const char* const kNoCopyPaths[] = {kTmpDir, "Downloads", "Cache", "MyFiles"};
// The base names of files and directories directory under the user data
// directory.
const char* const kCopyUserDataPaths[] = {"First Run"};
// Flag values for `switches::kForceBrowserDataMigrationForTesting`.
const char kBrowserDataMigrationForceSkip[] = "force-skip";
const char kBrowserDataMigrationForceMigration[] = "force-migration";

// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool CopyTargetItem(const BrowserDataMigrator::TargetItem& item,
                    const base::FilePath& dest) {
  if (item.is_directory) {
    if (base::CopyDirectory(item.path, dest, true /* recursive */))
      return true;
  } else {
    if (base::CopyFile(item.path, dest))
      return true;
  }

  PLOG(ERROR) << "Copy failed for " << item.path;
  return false;
}

void OnRestartRequestResponse(bool result) {
  if (!result) {
    LOG(ERROR) << "SessionManagerClient::RequestBrowserDataMigration failed.";
    return;
  }

  chrome::AttemptRestart();
}

// This will be posted with `IsMigrationRequiredOnWorker()` as the reply on UI
// thread or called directly from `MaybeRestartToMigrate()`.
void MaybeRestartToMigrateCallback(const AccountId& account_id,
                                   bool is_required) {
  if (!is_required)
    return;
  SessionManagerClient::Get()->RequestBrowserDataMigration(
      cryptohome::CreateAccountIdentifierFromAccountId(account_id),
      base::BindOnce(&OnRestartRequestResponse));
}
}  // namespace

BrowserDataMigrator::TargetItem::TargetItem(base::FilePath path,
                                            ItemType item_type)
    : path(path), is_directory(item_type == ItemType::kDirectory) {}

bool BrowserDataMigrator::TargetItem::operator==(const TargetItem& rhs) const {
  return this->path == rhs.path && this->is_directory == rhs.is_directory;
}

BrowserDataMigrator::TargetInfo::TargetInfo() : total_byte_count(0) {}
BrowserDataMigrator::TargetInfo::TargetInfo(const TargetInfo&) = default;
BrowserDataMigrator::TargetInfo::~TargetInfo() = default;

// static
void BrowserDataMigrator::MaybeRestartToMigrate(
    const UserContext& user_context) {
  const AccountId account_id = user_context.GetAccountId();
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  // Check if lacros is enabled. If not immediately return.
  if (!crosapi::browser_util::IsLacrosEnabledWithUser(user))
    return;

  // Check if the switch for testing is present.
  const std::string force_migration_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kForceBrowserDataMigrationForTesting);
  if (force_migration_switch == kBrowserDataMigrationForceSkip)
    return;
  if (force_migration_switch == kBrowserDataMigrationForceMigration) {
    MaybeRestartToMigrateCallback(account_id, true /* is_required */);
    return;
  }

  // Browser data migration is only available for Googlers at the moment.
  if (!gaia::IsGoogleInternalAccountEmail(account_id.GetUserEmail()))
    return;

  const std::string user_id_hash = user_context.GetUserIDHash();
  if (crosapi::browser_util::IsDataWipeRequired(user_id_hash)) {
    // If data wipe is required, no need for a further check to determine if
    // lacros data dir exists or not.
    MaybeRestartToMigrateCallback(account_id, true /* is_required */);
    return;
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Could not get the original user data dir path.";
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataMigrator::IsMigrationRequiredOnWorker,
                     user_data_dir, user_id_hash),
      base::BindOnce(&MaybeRestartToMigrateCallback, account_id));
}

// static
// Returns true if "lacros user data dir doesn't exist".
bool BrowserDataMigrator::IsMigrationRequiredOnWorker(
    base::FilePath user_data_dir,
    const std::string& user_id_hash) {
  // Use `GetUserProfileDir()` to manually get base name for profile dir so that
  // this method can be called even before user profile is created.
  base::FilePath profile_data_dir =
      user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));

  return !base::DirectoryExists(profile_data_dir.Append(kLacrosDir));
}

void BrowserDataMigrator::Migrate(const std::string& user_id_hash,
                                  base::OnceClosure callback) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR)
        << "Could not get the original user data dir path. Aborting migration.";
    RecordStatus(FinalStatus::kGetPathFailed);
    std::move(callback).Run();
    return;
  }
  base::FilePath profile_data_dir =
      user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));
  std::unique_ptr<BrowserDataMigrator> browser_data_migrator =
      std::make_unique<BrowserDataMigrator>(profile_data_dir);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataMigrator::MigrateInternal,
                     std::move(browser_data_migrator)),
      base::BindOnce(&BrowserDataMigrator::MigrateInternalFinishedUIThread,
                     std::move(callback), user_id_hash));
}

BrowserDataMigrator::BrowserDataMigrator(const base::FilePath& from)
    : from_dir_(from),
      to_dir_(from.Append(kLacrosDir)),
      tmp_dir_(from.Append(kTmpDir)) {}

BrowserDataMigrator::~BrowserDataMigrator() = default;

// static
void BrowserDataMigrator::RecordStatus(const FinalStatus& final_status,
                                       const TargetInfo* target_info,
                                       const base::ElapsedTimer* timer) {
  // Record final status enum.
  UMA_HISTOGRAM_ENUMERATION(kFinalStatus, final_status);

  if (!target_info)
    return;
  // Record byte size. Range 0 ~ 10GB in MBs.
  UMA_HISTOGRAM_CUSTOM_COUNTS(kCopiedDataSize,
                              target_info->total_byte_count / 1024 / 1024, 1,
                              10000, 100);

  if (!timer || final_status != FinalStatus::kSuccess)
    return;
  // Record elapsed time only for successful cases.
  UMA_HISTOGRAM_MEDIUM_TIMES(kTotalTime, timer->Elapsed());
}

// TODO(crbug.com/1178702): Once testing phase is over and lacros becomes the
// only web browser, update the underlying logic of migration from copy to move.
// Note that during testing phase we are copying files and leaving files in
// original location intact. We will allow these two states to diverge.
BrowserDataMigrator::MigrationResult BrowserDataMigrator::MigrateInternal() {
  ResultValue data_wipe_result = ResultValue::kSkipped;

  if (base::DirectoryExists(to_dir_)) {
    if (!base::DeletePathRecursively(to_dir_)) {
      RecordStatus(FinalStatus::kDataWipeFailed);
      return {ResultValue::kFailed, ResultValue::kFailed};
    }
    data_wipe_result = ResultValue::kSucceeded;
  }

  // Check if tmp directory already exists and delete if it does.
  if (base::PathExists(tmp_dir_)) {
    LOG(WARNING) << kTmpDir
                 << " already exists indicating migration was aborted on the"
                    "previous attempt.";
    if (!base::DeletePathRecursively(tmp_dir_)) {
      PLOG(ERROR) << "Failed to delete tmp dir";
      RecordStatus(FinalStatus::kDeleteTmpDirFailed);
      return {data_wipe_result, ResultValue::kFailed};
    }
  }

  TargetInfo target_info = GetTargetInfo();
  base::ElapsedTimer timer;

  if (!HasEnoughDiskSpace(target_info)) {
    RecordStatus(FinalStatus::kNotEnoughSpace, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  if (!CopyToTmpDir(target_info)) {
    if (base::PathExists(tmp_dir_)) {
      base::DeletePathRecursively(tmp_dir_);
    }
    RecordStatus(FinalStatus::kCopyFailed, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  if (!MoveTmpToTargetDir()) {
    if (base::PathExists(tmp_dir_)) {
      base::DeletePathRecursively(tmp_dir_);
    }
    RecordStatus(FinalStatus::kMoveFailed, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  LOG(WARNING) << "BrowserDataMigrator::Migrate took "
               << timer.Elapsed().InMilliseconds() << " ms and migrated "
               << target_info.total_byte_count / (1000 * 1000) << " MBs.";
  RecordStatus(FinalStatus::kSuccess, &target_info, &timer);
  return {data_wipe_result, ResultValue::kSucceeded};
}

void BrowserDataMigrator::MigrateInternalFinishedUIThread(
    base::OnceClosure callback,
    const std::string& user_id_hash,
    MigrationResult result) {
  if (result.data_wipe == ResultValue::kSucceeded) {
    crosapi::browser_util::RecordDataVer(g_browser_process->local_state(),
                                         user_id_hash,
                                         version_info::GetVersion());
  }

  if (result.data_migration == ResultValue::kSucceeded) {
    // If we did a migration, then we should set kClearUserDataDir1Pref. Note
    // that if we did the migration, then the new user-data-dir has the ash
    // profile as the main lacros profile.
    //
    // We only needed to delete old user-data-dirs because of the possibility
    // that the main profile might not match the ash profile.
    //
    // TODO(https://crbug.com/1197220): Set the preference
    // kClearUserDataDir1Pref to true. It's unclear whether the profile is
    // ready for use at this point in the startup cycle.
  }
  std::move(callback).Run();
}

BrowserDataMigrator::TargetInfo BrowserDataMigrator::GetTargetInfo() const {
  TargetInfo target_info;

  base::FileEnumerator enumerator(from_dir_, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    if (base::Contains(kNoCopyPaths, entry.BaseName().value())) {
      // Skip if the base name is present in `kNoCopyPaths`.
      continue;
    }

    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    if (S_ISREG(info.stat().st_mode)) {
      target_info.profile_data_items.emplace_back(
          TargetItem{entry, TargetItem::ItemType::kFile});
      target_info.total_byte_count += info.GetSize();
    } else {
      // Treat symlink the same as directory since even if it points to a file,
      // both `ComputeDirectorySize()` and `CopyDirectory()` can be used.
      target_info.profile_data_items.emplace_back(
          TargetItem{entry, TargetItem::ItemType::kDirectory});
      target_info.total_byte_count += base::ComputeDirectorySize(entry);
    }
  }

  // Copy files directly under user data directory.
  for (auto* copy_path : kCopyUserDataPaths) {
    base::FilePath entry = from_dir_.DirName().Append(copy_path);
    if (base::PathExists(entry)) {
      target_info.user_data_items.emplace_back(
          TargetItem{entry, TargetItem::ItemType::kFile});
      target_info.total_byte_count += base::ComputeDirectorySize(entry);
    }
  }

  return target_info;
}

bool BrowserDataMigrator::HasEnoughDiskSpace(
    const TargetInfo& target_info) const {
  const int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(from_dir_);
  if (free_disk_space < target_info.total_byte_count) {
    LOG(ERROR) << "Aborting migration. Need " << target_info.total_byte_count
               << " bytes but only have " << free_disk_space << " bytes left.";
    return false;
  }
  return true;
}

bool BrowserDataMigrator::CopyToTmpDir(const TargetInfo& target_info) const {
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(tmp_dir_.Append(kLacrosProfilePath),
                                        &error)) {
    PLOG(ERROR) << "CreateDirectoryFailed " << error;
    // Maps to histogram enum `PlatformFileError`.
    UMA_HISTOGRAM_ENUMERATION(kCreateDirectoryFail, -error,
                              -base::File::FILE_ERROR_MAX);
    return false;
  }

  for (const auto& target_item : target_info.profile_data_items) {
    base::FilePath dest =
        tmp_dir_.Append(kLacrosProfilePath).Append(target_item.path.BaseName());

    if (!CopyTargetItem(target_item, dest))
      return false;
  }

  for (const auto& target_item : target_info.user_data_items) {
    base::FilePath dest = tmp_dir_.Append(target_item.path.BaseName());

    if (!CopyTargetItem(target_item, dest))
      return false;
  }

  return true;
}

bool BrowserDataMigrator::MoveTmpToTargetDir() const {
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(to_dir_.DirName(), &error)) {
    LOG(ERROR) << "CreateDirectoryFailed " << error;
    return false;
  }

  if (!base::Move(tmp_dir_, to_dir_)) {
    PLOG(ERROR) << "Move failed";
    return false;
  }

  return true;
}
}  // namespace ash
