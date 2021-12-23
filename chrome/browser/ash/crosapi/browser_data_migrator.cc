// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/callback_helpers.h"
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
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_constants.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "components/version_info/version_info.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {
// The base names of files and directories directly under the original profile
// data directory that does not need to be copied nor need to remain in ash e.g.
// cache data.
constexpr const char* const kNoCopyPathsDeprecated[] = {kTmpDir, "Cache"};
constexpr const char* const kNoCopyPaths[] = {
    kTmpDir,
    "Cache",
    "Code Cache",
    "crash",
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
                                               "Cookies",
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

CancelFlag::CancelFlag() : cancelled_(false) {}
CancelFlag::~CancelFlag() = default;

BrowserDataMigratorImpl::TargetInfo::TargetInfo()
    : ash_data_size(0),
      no_copy_data_size(0),
      lacros_data_size(0),
      common_data_size(0) {}
BrowserDataMigratorImpl::TargetInfo::TargetInfo(TargetInfo&&) = default;
BrowserDataMigratorImpl::TargetInfo::~TargetInfo() = default;

BrowserDataMigratorImpl::TargetItem::TargetItem(base::FilePath path,
                                                int64_t size,
                                                ItemType item_type)
    : path(path), size(size), is_directory(item_type == ItemType::kDirectory) {}

bool BrowserDataMigratorImpl::TargetItem::operator==(
    const TargetItem& rhs) const {
  return this->path == rhs.path && this->size == rhs.size &&
         this->is_directory == rhs.is_directory;
}

int64_t BrowserDataMigratorImpl::TargetInfo::TotalCopySize() const {
  return lacros_data_size + common_data_size;
}

int64_t BrowserDataMigratorImpl::TargetInfo::TotalDirSize() const {
  return no_copy_data_size + ash_data_size + lacros_data_size +
         common_data_size;
}

// static
bool BrowserDataMigratorImpl::MaybeRestartToMigrate(
    const AccountId& account_id,
    const std::string& user_id_hash) {
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
  if (!crosapi::browser_util::IsLacrosEnabledForMigration(user)) {
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
      cancel_flag_(base::MakeRefCounted<CancelFlag>()),
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
void BrowserDataMigratorImpl::RecordStatus(const FinalStatus& final_status,
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
BrowserDataMigratorImpl::MigrationResult
BrowserDataMigratorImpl::MigrateInternal(
    const base::FilePath& original_user_dir,
    std::unique_ptr<MigrationProgressTracker> progress_tracker,
    scoped_refptr<CancelFlag> cancel_flag) {
  ResultValue data_wipe_result = ResultValue::kSkipped;

  const base::FilePath tmp_dir = original_user_dir.Append(kTmpDir);
  const base::FilePath new_user_dir = original_user_dir.Append(kLacrosDir);

  if (base::DirectoryExists(new_user_dir)) {
    if (!base::DeletePathRecursively(new_user_dir)) {
      PLOG(ERROR) << "Deleting " << new_user_dir.value() << " failed: ";
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
  progress_tracker->SetTotalSizeToCopy(target_info.TotalCopySize());

  base::ElapsedTimer timer;

  if (!HasEnoughDiskSpace(target_info, original_user_dir, Mode::kCopy)) {
    RecordStatus(FinalStatus::kNotEnoughSpace, &target_info);
    return {data_wipe_result, ResultValue::kFailed};
  }

  // Copy files to `tmp_dir`.
  if (!SetupTmpDir(target_info, original_user_dir, tmp_dir, cancel_flag.get(),
                   progress_tracker.get())) {
    if (base::PathExists(tmp_dir)) {
      base::DeletePathRecursively(tmp_dir);
    }
    if (cancel_flag->IsSet()) {
      LOG(WARNING) << "Migration was cancelled.";
      RecordStatus(FinalStatus::kCancelled, &target_info);
      return {data_wipe_result, ResultValue::kCancelled};
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

  LOG(WARNING) << "BrowserDataMigratorImpl::Migrate took "
               << timer.Elapsed().InMilliseconds() << " ms and migrated "
               << target_info.TotalCopySize() / (1024 * 1024) << " MB.";
  RecordStatus(FinalStatus::kSuccess, &target_info, &timer);
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
// Copies `item` to location pointed by `dest`. Returns true on success and
// false on failure.
bool BrowserDataMigratorImpl::CopyTargetItem(
    const BrowserDataMigratorImpl::TargetItem& item,
    const base::FilePath& dest,
    CancelFlag* cancel_flag,
    MigrationProgressTracker* progress_tracker) {
  if (cancel_flag->IsSet())
    return false;

  if (item.is_directory) {
    if (CopyDirectory(item.path, dest, cancel_flag, progress_tracker))
      return true;
  } else {
    if (base::CopyFile(item.path, dest)) {
      progress_tracker->UpdateProgress(item.size);
      return true;
    }
  }

  PLOG(ERROR) << "Copy failed for " << item.path;
  return false;
}

// static
BrowserDataMigratorImpl::TargetInfo BrowserDataMigratorImpl::GetTargetInfo(
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
bool BrowserDataMigratorImpl::HasEnoughDiskSpace(
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
bool BrowserDataMigratorImpl::CopyDirectory(
    const base::FilePath& from_path,
    const base::FilePath& to_path,
    CancelFlag* cancel_flag,
    MigrationProgressTracker* progress_tracker) {
  if (cancel_flag->IsSet())
    return false;

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
    if (cancel_flag->IsSet())
      return false;

    const base::FileEnumerator::FileInfo& info = enumerator.GetInfo();

    // Only copy a file or a dir i.e. skip other types like symlink since
    // copying those might introdue a security risk.
    if (S_ISREG(info.stat().st_mode)) {
      if (!base::CopyFile(entry, to_path.Append(entry.BaseName())))
        return false;

      progress_tracker->UpdateProgress(info.GetSize());
    } else if (S_ISDIR(info.stat().st_mode)) {
      if (!CopyDirectory(entry, to_path.Append(entry.BaseName()), cancel_flag,
                         progress_tracker)) {
        return false;
      }
    }
  }

  return true;
}

// static
bool BrowserDataMigratorImpl::CopyTargetItems(
    const base::FilePath& to_dir,
    const std::vector<TargetItem>& items,
    CancelFlag* cancel_flag,
    int64_t items_size,
    base::StringPiece category_name,
    MigrationProgressTracker* progress_tracker) {
  base::ElapsedTimer timer;
  for (const auto& item : items) {
    if (cancel_flag->IsSet())
      return false;

    if (!CopyTargetItem(item, to_dir.Append(item.path.BaseName()), cancel_flag,
                        progress_tracker)) {
      return false;
    }
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
bool BrowserDataMigratorImpl::SetupTmpDir(
    const TargetInfo& target_info,
    const base::FilePath& from_dir,
    const base::FilePath& tmp_dir,
    CancelFlag* cancel_flag,
    MigrationProgressTracker* progress_tracker) {
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
                       target_info.lacros_data_items, cancel_flag,
                       target_info.lacros_data_size, kLacrosCategory,
                       progress_tracker))
    return false;
  // Copy common items.
  if (!CopyTargetItems(tmp_dir.Append(kLacrosProfilePath),
                       target_info.common_data_items, cancel_flag,
                       target_info.common_data_size, kCommonCategory,
                       progress_tracker))
    return false;

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
  base::DictionaryValue* dict = update.Get();
  dict->SetKey(user_id_hash, base::Value(count));
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
  base::DictionaryValue* dict = update.Get();
  dict->RemoveKey(user_id_hash);
}

// static
void BrowserDataMigratorImpl::DryRunToCollectUMA(
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
void BrowserDataMigratorImpl::RecordTargetItemSizes(
    const std::vector<TargetItem>& items) {
  for (auto& item : items)
    browser_data_migrator_util::RecordUserDataSize(item.path, item.size);
}
}  // namespace ash
