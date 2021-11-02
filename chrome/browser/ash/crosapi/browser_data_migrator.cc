// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
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
// data directory that does not need to be copied nor need to remain in ash e.g.
// cache data.
constexpr const char* const kNoCopyPathsDeprecated[] = {kTmpDir, "Cache"};
constexpr const char* const kNoCopyPaths[] = {
    kTmpDir,
    "Cache",
    "Code Cache",
    "blob_storage",
    "GCache",
    "data_reduction_proxy_leveldb",
    "previews_opt_out.db",
    "Download Service",
    "Network Persistent State",
    "Reporting and NEL",
    "TransportSecurity",
    "optimization_guide_hint_cache_store",
    "Site Characteristics Database",
    "Network Action Predictor"};
// The base names of files and directories that should remain in ash data
// directory.
constexpr const char* const kAshDataPathsDeprecated[]{"Downloads", "MyFiles"};
constexpr const char* const kAshDataPaths[] = {"FullRestoreData",
                                               "Downloads",
                                               "MyFiles",
                                               "arc.apps",
                                               "crostini.icons",
                                               "PreferredApps",
                                               "autobrightness",
                                               "extension_install_log",
                                               "google-assistant-library",
                                               "login-times",
                                               "logout-times",
                                               "structured_metrics",
                                               "PrintJobDatabase",
                                               "PPDCache",
                                               "BudgetDatabase",
                                               "RLZ Data",
                                               "app_ranker.pb",
                                               "zero_state_group_ranker.pb",
                                               "zero_state_local_files.pb"};
// The base names of files/dirs that are needed only by the browser part of
// chrome i.e. data that should be moved to lacros.
constexpr const char* const kLacrosDataPathsDeprecated[]{"Bookmarks"};
constexpr const char* const kLacrosDataPaths[]{"AutofillStrikeDatabase",
                                               "Bookmarks",
                                               "Extension Cookies",
                                               "Extension Rules",
                                               "Extension State",
                                               "Extensions",
                                               "Local App Settings",
                                               "Local Extension Settings",
                                               "Managed Extension Settings",
                                               "Sync App Settings",
                                               "DNR Extension Rules",
                                               "Favicons",
                                               "History",
                                               "Top Sites",
                                               "Shortcuts",
                                               "Sessions"};
// `First Run` is the only file that should be copied to lacros from user data
// directory (parent directory of profile directory).
const char* const kFirstRun = "First Run";
// Flag values for `switches::kForceBrowserDataMigrationForTesting`.
const char kBrowserDataMigrationForceSkip[] = "force-skip";
const char kBrowserDataMigrationForceMigration[] = "force-migration";
// The size of disk space that should be kept free after migration. This is
// important since crypotohome conducts an aggressive disk cleanup if free disk
// space becomes less than 768MB. The buffer is rounded up to 1GB.
const int64_t kBuffer = (int64_t)1024 * 1024 * 1024;
// Category names used for logging information about corresponding
// vector<TargetItem> in TargetInfo.
constexpr char kLacrosCategory[] = "lacros";
constexpr char kCommonCategory[] = "common";

// Enable this to turn on profile migration for non-googlers. Currently the
// feature is only limited to googlers only.
const base::Feature kLacrosProfileMigrationForAnyUser{
    "LacrosProfileMigrationForAnyUser", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable these to fallback to an older version of paths lists.
const base::Feature kLacrosProfileMigrationUseDeprecatedNoCopyPaths{
    "LacrosProfileMigrationUseDeprecatedNoCopyPaths",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kLacrosProfileMigrationUseDeprecatedAshDataPaths{
    "LacrosProfileMigrationUseDeprecatedAshDataPaths",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kLacrosProfileMigrationUseDeprecatedLacrosDataPaths{
    "LacrosProfileMigrationUseDeprecatedLacrosDataPaths",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Emergency switch to turn off profile migration via Finch.
const base::Feature kLacrosProfileMigrationForceOff{
    "LacrosProfileMigrationForceOff", base::FEATURE_DISABLED_BY_DEFAULT};

// Ensures that each path in UDD appears in one of `kNoCopyPaths`,
// `kAshDataPaths` or `kLacrosDataPaths`.
constexpr bool HasNoOverlapBetweenPathsSets() {
  for (const char* no_copy_path : kNoCopyPaths) {
    for (const char* ash_data_path : kAshDataPaths) {
      if (base::StringPiece(no_copy_path) == base::StringPiece(ash_data_path))
        return false;
    }
  }

  for (const char* ash_data_path : kAshDataPaths) {
    for (const char* lacros_data_path : kLacrosDataPaths) {
      if (base::StringPiece(ash_data_path) ==
          base::StringPiece(lacros_data_path))
        return false;
    }
  }

  for (const char* lacros_data_path : kLacrosDataPaths) {
    for (const char* no_copy_path : kNoCopyPaths) {
      if (base::StringPiece(lacros_data_path) ==
          base::StringPiece(no_copy_path))
        return false;
    }
  }

  return true;
}

static_assert(HasNoOverlapBetweenPathsSets(),
              "There must be no overlap between kNoCopyPaths, kAshDataPaths "
              "and kLacrosDataPaths");

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

base::span<const char* const> GetNoCopyDataPaths() {
  if (base::FeatureList::IsEnabled(
          kLacrosProfileMigrationUseDeprecatedNoCopyPaths)) {
    return base::make_span(kNoCopyPathsDeprecated);
  }
  return base::make_span(kNoCopyPaths);
}

base::span<const char* const> GetAshDataPaths() {
  if (base::FeatureList::IsEnabled(
          kLacrosProfileMigrationUseDeprecatedAshDataPaths)) {
    return base::make_span(kAshDataPathsDeprecated);
  }
  return base::make_span(kAshDataPaths);
}

base::span<const char* const> GetLacrosDataPaths() {
  if (base::FeatureList::IsEnabled(
          kLacrosProfileMigrationUseDeprecatedLacrosDataPaths)) {
    return base::make_span(kLacrosDataPathsDeprecated);
  }
  return base::make_span(kLacrosDataPaths);
}
}  // namespace

BrowserDataMigrator::TargetInfo::TargetInfo()
    : ash_data_size(0),
      no_copy_data_size(0),
      lacros_data_size(0),
      common_data_size(0) {}
BrowserDataMigrator::TargetInfo::TargetInfo(TargetInfo&&) = default;
BrowserDataMigrator::TargetInfo::~TargetInfo() = default;

BrowserDataMigrator::TargetItem::TargetItem(base::FilePath path,
                                            int64_t size,
                                            ItemType item_type)
    : path(path), size(size), is_directory(item_type == ItemType::kDirectory) {}

bool BrowserDataMigrator::TargetItem::operator==(const TargetItem& rhs) const {
  return this->path == rhs.path && this->size == rhs.size &&
         this->is_directory == rhs.is_directory;
}

int64_t BrowserDataMigrator::TargetInfo::TotalCopySize() const {
  return lacros_data_size + common_data_size;
}

int64_t BrowserDataMigrator::TargetInfo::TotalDirSize() const {
  return no_copy_data_size + ash_data_size + lacros_data_size +
         common_data_size;
}

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

  //  Currently we turn on profile migration only for Googlers.
  //  `kLacrosProfileMigrationForAnyUser` can be enabled to allow testing with
  //  non-googler accounts.
  if (!gaia::IsGoogleInternalAccountEmail(account_id.GetUserEmail()) &&
      !base::FeatureList::IsEnabled(kLacrosProfileMigrationForAnyUser))
    return;

  // Emergency switch to turn off profile migration. Turn this on via Finch in
  // case profile migration needs to be turned off after launch.
  if (base::FeatureList::IsEnabled(kLacrosProfileMigrationForceOff))
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
// Returns true if lacros user data dir doesn't exist.
bool BrowserDataMigrator::IsMigrationRequiredOnWorker(
    base::FilePath user_data_dir,
    const std::string& user_id_hash) {
  // Use `GetUserProfileDir()` to manually get base name for profile dir so that
  // this method can be called even before user profile is created.
  base::FilePath profile_data_dir =
      user_data_dir.Append(ProfileHelper::GetUserProfileDir(user_id_hash));

  return !base::DirectoryExists(profile_data_dir.Append(kLacrosDir));
}

// static
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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(&BrowserDataMigrator::MigrateInternal, profile_data_dir),
      base::BindOnce(&BrowserDataMigrator::MigrateInternalFinishedUIThread,
                     std::move(callback), user_id_hash));
}

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
                              target_info->TotalCopySize() / 1024 / 1024, 1,
                              10000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      kAshDataSize, target_info->ash_data_size / 1024 / 1024, 1, 10000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kLacrosDataSize,
                              target_info->lacros_data_size / 1024 / 1024, 1,
                              10000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kCommonDataSize,
                              target_info->common_data_size / 1024 / 1024, 1,
                              10000, 100);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kNoCopyDataSize,
                              target_info->no_copy_data_size / 1024 / 1024, 1,
                              10000, 100);

  if (!timer || final_status != FinalStatus::kSuccess)
    return;
  // Record elapsed time only for successful cases.
  UMA_HISTOGRAM_MEDIUM_TIMES(kTotalTime, timer->Elapsed());
}

// static
// TODO(crbug.com/1178702): Once testing phase is over and lacros becomes the
// only web browser, update the underlying logic of migration from copy to move.
// Note that during testing phase we are copying files and leaving files in
// original location intact. We will allow these two states to diverge.
BrowserDataMigrator::MigrationResult BrowserDataMigrator::MigrateInternal(
    const base::FilePath& original_user_dir) {
  ResultValue data_wipe_result = ResultValue::kSkipped;

  const base::FilePath tmp_dir = original_user_dir.Append(kTmpDir);
  const base::FilePath new_user_dir = original_user_dir.Append(kLacrosDir);

  if (base::DirectoryExists(new_user_dir)) {
    if (!base::DeletePathRecursively(new_user_dir)) {
      RecordStatus(FinalStatus::kDataWipeFailed);
      return {ResultValue::kFailed, ResultValue::kFailed};
    }
    data_wipe_result = ResultValue::kSucceeded;
  }

  // Check if tmp directory already exists and delete if it does.
  if (base::PathExists(tmp_dir)) {
    LOG(WARNING) << kTmpDir
                 << " already exists indicating migration was aborted on the"
                    "previous attempt.";
    if (!base::DeletePathRecursively(tmp_dir)) {
      PLOG(ERROR) << "Failed to delete tmp dir";
      RecordStatus(FinalStatus::kDeleteTmpDirFailed);
      return {data_wipe_result, ResultValue::kFailed};
    }
  }

  TargetInfo target_info = GetTargetInfo(original_user_dir);
  base::ElapsedTimer timer;

  if (!HasEnoughDiskSpace(target_info, original_user_dir, Mode::kCopy)) {
    RecordStatus(FinalStatus::kNotEnoughSpace, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  if (!IsMigrationSmallEnough(target_info)) {
    RecordStatus(FinalStatus::kSizeLimitExceeded, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  // Copy files to `tmp_dir`.
  if (!SetupTmpDir(target_info, original_user_dir, tmp_dir)) {
    if (base::PathExists(tmp_dir)) {
      base::DeletePathRecursively(tmp_dir);
    }
    RecordStatus(FinalStatus::kCopyFailed, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  // Move `tmp_dir` to `new_user_dir`.
  if (!base::Move(tmp_dir, new_user_dir)) {
    PLOG(ERROR) << "Move failed";
    if (base::PathExists(tmp_dir)) {
      base::DeletePathRecursively(tmp_dir);
    }
    RecordStatus(FinalStatus::kMoveFailed, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  LOG(WARNING) << "BrowserDataMigrator::Migrate took "
               << timer.Elapsed().InMilliseconds() << " ms and migrated "
               << target_info.TotalCopySize() / (1024 * 1024) << " MB.";
  RecordStatus(FinalStatus::kSuccess, &target_info, &timer);
  return {data_wipe_result, ResultValue::kSucceeded};
}

// static
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

// static
// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool BrowserDataMigrator::CopyTargetItem(
    const BrowserDataMigrator::TargetItem& item,
    const base::FilePath& dest) {
  if (item.is_directory) {
    if (CopyDirectory(item.path, dest))
      return true;
  } else {
    if (base::CopyFile(item.path, dest))
      return true;
  }

  PLOG(ERROR) << "Copy failed for " << item.path;
  return false;
}

// static
BrowserDataMigrator::TargetInfo BrowserDataMigrator::GetTargetInfo(
    const base::FilePath& original_user_dir) {
  TargetInfo target_info;
  const base::span<const char* const> no_copy_data_paths = GetNoCopyDataPaths();
  const base::span<const char* const> ash_data_paths = GetAshDataPaths();
  const base::span<const char* const> lacros_data_paths = GetLacrosDataPaths();

  base::FileEnumerator enumerator(original_user_dir, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();
    int64_t size;
    TargetItem::ItemType item_type;
    if (S_ISREG(info.stat().st_mode)) {
      size = info.GetSize();
      item_type = TargetItem::ItemType::kFile;
    } else if (S_ISDIR(info.stat().st_mode)) {
      size =
          browser_data_migrator_util::ComputeDirectorySizeWithoutLinks(entry);
      item_type = TargetItem::ItemType::kDirectory;
    } else {
      // Skip if `entry` is not a file or directory such as a symlink.
      continue;
    }

    if (base::Contains(ash_data_paths, entry.BaseName().value())) {
      target_info.ash_data_items.emplace_back(
          TargetItem{entry, size, item_type});
      target_info.ash_data_size += size;
    } else if (base::Contains(no_copy_data_paths, entry.BaseName().value())) {
      target_info.no_copy_data_items.emplace_back(
          TargetItem{entry, size, item_type});
      target_info.no_copy_data_size += size;
    } else if (base::Contains(lacros_data_paths, entry.BaseName().value())) {
      // Items that should be moved to lacros.
      target_info.lacros_data_items.emplace_back(
          TargetItem{entry, size, item_type});
      target_info.lacros_data_size += size;
    } else {
      // Items that are not explicitly ash, no_copy or lacros are put into
      // common category.
      target_info.common_data_items.emplace_back(
          TargetItem{entry, size, item_type});
      target_info.common_data_size += size;
    }
  }

  return target_info;
}

// static
bool BrowserDataMigrator::HasEnoughDiskSpace(
    const TargetInfo& target_info,
    const base::FilePath& original_user_dir,
    Mode mode) {
  const int64_t free_disk_space =
      base::SysInfo::AmountOfFreeDiskSpace(original_user_dir);

  int64_t required_space;
  switch (mode) {
    case Mode::kMove:
      required_space = target_info.common_data_size;
      break;
    case Mode::kDeleteAndCopy:
      required_space =
          target_info.TotalCopySize() - target_info.no_copy_data_size;
      break;
    case Mode::kDeleteAndMove:
      required_space =
          target_info.common_data_size - target_info.no_copy_data_size;
      break;
    case Mode::kCopy:
    default:
      DCHECK_EQ(mode, Mode::kCopy);
      required_space = target_info.TotalCopySize();
      break;
  }

  if (free_disk_space < required_space + kBuffer) {
    LOG(ERROR) << "Aborting migration. Need " << required_space
               << " bytes but only have " << free_disk_space << " bytes left.";
    return false;
  }

  return true;
}

// static
bool BrowserDataMigrator::IsMigrationSmallEnough(
    const TargetInfo& target_info) {
  constexpr int64_t max_migration_size = (int64_t)4 * 1024 * 1024 * 1024;
  if (target_info.TotalCopySize() > max_migration_size) {
    LOG(ERROR) << "Aborting migration because the data size is too large for "
                  "migration: "
               << target_info.TotalCopySize() << " bytes.";
    return false;
  }

  return true;
}

// static
bool BrowserDataMigrator::CopyDirectory(const base::FilePath& from_path,
                                        const base::FilePath& to_path) {
  if (!base::PathExists(to_path) && !base::CreateDirectory(to_path)) {
    PLOG(ERROR) << "CreateDirectory() failed for " << to_path.value();
    return false;
  }

  base::FileEnumerator enumerator(from_path, false /* recursive */,
                                  base::FileEnumerator::FILES |
                                      base::FileEnumerator::DIRECTORIES |
                                      base::FileEnumerator::SHOW_SYM_LINKS);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();

    // Only copy a file or a dir i.e. skip other types like symlink since
    // copying those might introdue a security risk.
    if (S_ISREG(info.stat().st_mode)) {
      if (!base::CopyFile(entry, to_path.Append(entry.BaseName())))
        return false;
    } else if (S_ISDIR(info.stat().st_mode)) {
      if (!CopyDirectory(entry, to_path.Append(entry.BaseName())))
        return false;
    }
  }

  return true;
}

bool BrowserDataMigrator::CopyTargetItems(const base::FilePath& to_dir,
                                          const std::vector<TargetItem>& items,
                                          int64_t items_size,
                                          base::StringPiece category_name) {
  base::ElapsedTimer timer;
  for (const auto& item : items) {
    if (!CopyTargetItem(item, to_dir.Append(item.path.BaseName())))
      return false;
  }
  base::TimeDelta elapsed_time = timer.Elapsed();
  // TODO(crbug.com/1178702): Once BrowserDataMigrator stabilises, reduce the
  // log level to VLOG(1).
  LOG(WARNING) << "Copied " << items_size / (1024 * 1024) << " MB of "
               << category_name << " data and it took "
               << elapsed_time.InMilliseconds() << " ms.";

  if (category_name == kLacrosCategory) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kLacrosDataTime, elapsed_time);
  } else if (category_name == kCommonCategory) {
    UMA_HISTOGRAM_MEDIUM_TIMES(kCommonDataTime, elapsed_time);
  } else {
    NOTREACHED();
  }

  return true;
}

// static
bool BrowserDataMigrator::SetupTmpDir(const TargetInfo& target_info,
                                      const base::FilePath& from_dir,
                                      const base::FilePath& tmp_dir) {
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(tmp_dir.Append(kLacrosProfilePath),
                                        &error)) {
    PLOG(ERROR) << "CreateDirectoryFailed " << error;
    // Maps to histogram enum `PlatformFileError`.
    UMA_HISTOGRAM_ENUMERATION(kCreateDirectoryFail, -error,
                              -base::File::FILE_ERROR_MAX);
    return false;
  }

  // Copy lacros items.
  if (!CopyTargetItems(tmp_dir.Append(kLacrosProfilePath),
                       target_info.lacros_data_items,
                       target_info.lacros_data_size, kLacrosCategory))
    return false;
  // Copy common items.
  if (!CopyTargetItems(tmp_dir.Append(kLacrosProfilePath),
                       target_info.common_data_items,
                       target_info.common_data_size, kCommonCategory))
    return false;

  // Copy `First Run` in user data directory.
  const base::FilePath first_run_file = from_dir.DirName().Append(kFirstRun);
  if (base::PathExists(first_run_file)) {
    if (!base::CopyFile(first_run_file, tmp_dir.Append(kFirstRun)))
      return false;
  }

  return true;
}

// static
void BrowserDataMigrator::DryRunToCollectUMA(
    const base::FilePath& profile_data_dir) {
  TargetInfo target_info = GetTargetInfo(profile_data_dir);

  base::UmaHistogramCustomCounts(kDryRunNoCopyDataSize,
                                 target_info.no_copy_data_size / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunAshDataSize,
                                 target_info.ash_data_size / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunLacrosDataSize,
                                 target_info.lacros_data_size / 1024 / 1024, 1,
                                 10000, 100);
  base::UmaHistogramCustomCounts(kDryRunCommonDataSize,
                                 target_info.common_data_size / 1024 / 1024, 1,
                                 10000, 100);

  browser_data_migrator_util::RecordTotalSize(target_info.TotalDirSize());

  RecordTargetItemSizes(target_info.no_copy_data_items);
  RecordTargetItemSizes(target_info.ash_data_items);
  RecordTargetItemSizes(target_info.lacros_data_items);
  RecordTargetItemSizes(target_info.common_data_items);

  base::UmaHistogramBoolean(
      kDryRunCopyMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(target_info, profile_data_dir, Mode::kCopy));
  base::UmaHistogramBoolean(
      kDryRunMoveMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(target_info, profile_data_dir, Mode::kMove));
  base::UmaHistogramBoolean(
      kDryRunDeleteAndCopyMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(target_info, profile_data_dir, Mode::kDeleteAndCopy));
  base::UmaHistogramBoolean(
      kDryRunDeleteAndMoveMigrationHasEnoughDiskSpace,
      HasEnoughDiskSpace(target_info, profile_data_dir, Mode::kDeleteAndMove));
}

// staic
void BrowserDataMigrator::RecordTargetItemSizes(
    const std::vector<TargetItem>& items) {
  for (auto& item : items)
    browser_data_migrator_util::RecordUserDataSize(item.path, item.size);
}
}  // namespace ash
