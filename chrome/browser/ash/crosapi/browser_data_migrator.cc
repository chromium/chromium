// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/common/chrome_paths.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {
// The name of temporary directory that will store copies of files from user
// data directory. At the end of the migration, it will be moved to the
// appropriate destination.
constexpr char kTmpDir[] = "browser_data_migrator";
// The base names of files and directories directly under the original user data
// directory that should not be copied. e.g. caches or files only needed by ash.
const char* const kNoCopyPaths[] = {kTmpDir, "Downloads", "Cache"};
}  // namespace

BrowserDataMigrator::TargetInfo::TargetInfo() : total_byte_count(0) {}
BrowserDataMigrator::TargetInfo::TargetInfo(const TargetInfo&) = default;
BrowserDataMigrator::TargetInfo::~TargetInfo() = default;

// static
void BrowserDataMigrator::MaybeMigrate(const UserContext& user_context,
                                       base::OnceClosure callback) {
  // Get the current user.
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(user_context.GetAccountId());
  if (!user || !IsMigrationRequiredOnUI(user)) {
    // If lacros isn't enabled, skip migration and move on to the next step.
    std::move(callback).Run();
    return;
  }

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Could not get the original UDD path. Aborting migration.";
    std::move(callback).Run();
    return;
  }

  base::FilePath profile_data_dir = user_data_dir.Append(
      base::StringPrintf("u-%s", user_context.GetUserIDHash().c_str()));

  std::unique_ptr<BrowserDataMigrator> browser_data_migrator =
      std::make_unique<BrowserDataMigrator>(profile_data_dir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BrowserDataMigrator::MigrateInternal,
                     std::move(browser_data_migrator)),
      base::BindOnce(&BrowserDataMigrator::MigrateInternalFinishedUIThread,
                     std::move(callback)));
}

// static
bool BrowserDataMigrator::IsMigrationRequiredOnUI(
    const user_manager::User* user) {
  return crosapi::browser_util::IsLacrosEnabledWithUser(user);
}

BrowserDataMigrator::BrowserDataMigrator(const base::FilePath& from)
    : from_dir_(from),
      to_dir_(from.Append(kLacrosProfileDir)),
      tmp_dir_(from.Append(kTmpDir)) {}

BrowserDataMigrator::~BrowserDataMigrator() = default;

// TODO(crbug.com/1178702): Once testing phase is over and lacros become the
// only web browser, update the underlying logic of migration from copy to move.
// Note that during testing phase we are copying files and leaving files in
// original location intact. We will allow these two states to diverge.
bool BrowserDataMigrator::MigrateInternal() {
  if (!IsMigrationRequiredOnWorker())
    return false;

  base::ElapsedTimer timer;

  // Check if tmp directory already exists and delete if it does.
  if (base::PathExists(tmp_dir_)) {
    LOG(WARNING) << kTmpDir
                 << " already exists indicating migration was aborted on the"
                    "previous attempt.";
    if (!base::DeletePathRecursively(tmp_dir_)) {
      return false;
    }
  }

  TargetInfo target_info = GetTargetInfo();

  if (!HasEnoughDiskSpace(target_info))
    return false;

  if (!CopyToTmpDir(target_info)) {
    base::DeletePathRecursively(tmp_dir_);
    return false;
  }

  if (!MoveTmpToTargetDir()) {
    base::DeletePathRecursively(tmp_dir_);
    return false;
  }

  // TODO(crbug.com/1178702): Add UMA data collection here for success status,
  // data size and elapsed time.
  LOG(WARNING) << "BrowserDataMigrator::Migrate took "
               << timer.Elapsed().InMilliseconds() << " ms and migrated "
               << target_info.total_byte_count / (1000 * 1000) << " MBs.";
  return true;
}

void BrowserDataMigrator::MigrateInternalFinishedUIThread(
    base::OnceClosure callback,
    bool did_migrate) {
  if (did_migrate) {
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

bool BrowserDataMigrator::IsMigrationRequiredOnWorker() const {
  // Migration is required if the user data directory for lacros hasn't been
  // created yet i.e. lacros has not been launched yet.
  return base::DirectoryExists(from_dir_) && !base::DirectoryExists(to_dir_);
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
      // Skip if the base name is present in kNoCopyPaths.
      continue;
    }

    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    if (S_ISREG(info.stat().st_mode)) {
      target_info.file_paths.emplace_back(entry);
      target_info.total_byte_count += info.GetSize();
    } else {
      // Treat symlink the same as directory since even if it points to a file,
      // both ComputeDirectorySize and CopyDirectory can be used.
      target_info.dir_paths.emplace_back(entry);
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
  if (!base::CreateDirectoryAndGetError(tmp_dir_, &error)) {
    PLOG(ERROR) << "CreateDirectoryFailed " << error;
    return false;
  }

  for (const auto& target_file : target_info.file_paths) {
    if (!base::CopyFile(target_file, tmp_dir_.Append(target_file.BaseName()))) {
      PLOG(ERROR) << "CopyFile failed for " << target_file;
      return false;
    }
  }

  for (const auto& target_dir : target_info.dir_paths) {
    if (!base::CopyDirectory(target_dir, tmp_dir_.Append(target_dir.BaseName()),
                             true /* recursive */)) {
      PLOG(ERROR) << "CopyDirectory failed for " << target_dir;
      return false;
    }
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
