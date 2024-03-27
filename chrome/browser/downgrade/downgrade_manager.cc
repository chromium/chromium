// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/downgrade_manager.h"

#include <iterator>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/enterprise_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/downgrade/downgrade_utils.h"
#include "chrome/browser/downgrade/snapshot_manager.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

namespace downgrade {

namespace {

bool g_snapshots_enabled_for_testing = false;

// Moves the contents of a User Data directory at |source| to |target|, with the
// exception of files/directories that should be left behind for a full data
// wipe. Returns no value if the target directory could not be created, or the
// number of items that could not be moved.
void MoveUserData(const base::FilePath& source, const base::FilePath& target) {
  // Returns true to exclude a file.
  auto exclusion_predicate =
      base::BindRepeating([](const base::FilePath& name) -> bool {
        // TODO(ydago): Share constants instead of hardcoding values here.
        static constexpr base::FilePath::StringPieceType kFilesToKeep[] = {
            FILE_PATH_LITERAL("browsermetrics"),
            FILE_PATH_LITERAL("crashpad"),
            FILE_PATH_LITERAL("first run"),
            FILE_PATH_LITERAL("last version"),
            FILE_PATH_LITERAL("lockfile"),
            FILE_PATH_LITERAL("snapshots"),
            FILE_PATH_LITERAL("stability"),
        };
        // Don't try to move the dir into which everything is being moved.
        if (name.FinalExtension() == kDowngradeDeleteSuffix)
          return true;
        return base::ranges::any_of(kFilesToKeep, [&name](const auto& keep) {
          return base::EqualsCaseInsensitiveASCII(name.value(), keep);
        });
      });
  auto result = MoveContents(source, target, std::move(exclusion_predicate));

  // Move the Last Version file last so that any crash before this point results
  // in a retry on the next launch.
  if (!result ||
      !MoveWithoutFallback(source.Append(kDowngradeLastVersionFile),
                           target.Append(kDowngradeLastVersionFile))) {
    // Attempt to delete Last Version if all else failed so that Chrome does not
    // continually attempt to perform a migration.
    base::DeleteFile(source.Append(kDowngradeLastVersionFile));
    // Inform system administrators that things have gone awry.
    SYSLOG(ERROR) << "Failed to perform User Data migration following a Chrome "
                     "version downgrade. Chrome will run with User Data from a "
                     "higher version and may behave unpredictably.";
    // At this point, Chrome will relaunch with --user-data-migrated. This
    // switch suppresses downgrade processing, so that launch will go through
    // normal startup.
  }
}

// Renames |disk_cache_dir| in its containing folder. If that fails, an attempt
// is made to move its contents.
void MoveCache(const base::FilePath& disk_cache_dir) {
  // A cache dir at the root of a volume is not supported.
  const base::FilePath parent = disk_cache_dir.DirName();
  if (parent == disk_cache_dir)
    return;

  // Move the cache within its parent directory from, for example, CacheDir
  // to CacheDir.CHROME_DELETE.
  const base::FilePath target =
      GetTempDirNameForDelete(parent, disk_cache_dir.BaseName());

  // A simple move succeeds in approx 2/3 of attempts.
  if (MoveWithoutFallback(disk_cache_dir, target))
    return;

  // The directory couldn't be moved whole-hog. Attempt a recursive move of its
  // contents. This succeeds in nearly all cases.
  MoveContents(disk_cache_dir, target, ExclusionPredicate());
}

// Deletes all subdirectories in |dir| named |name|*.CHROME_DELETE.
void DeleteAllRenamedUserDirectories(const base::FilePath& dir,
                                     const base::FilePath& name) {
  base::FilePath::StringType pattern = base::StrCat(
      {name.value(), FILE_PATH_LITERAL("*"), kDowngradeDeleteSuffix});
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::DIRECTORIES,
                                  pattern);
  for (base::FilePath to_delete = enumerator.Next(); !to_delete.empty();
       to_delete = enumerator.Next()) {
    base::DeletePathRecursively(to_delete);
  }
}

// Deletes all moved User Data, Snapshots and Cache directories for the given
// dirs.
void DeleteMovedUserData(const base::FilePath& user_data_dir,
                         const base::FilePath& disk_cache_dir) {
  DeleteAllRenamedUserDirectories(user_data_dir, user_data_dir.BaseName());
  DeleteAllRenamedUserDirectories(user_data_dir, base::FilePath(kSnapshotsDir));

  // Prior to Chrome M78, User Data was moved to a new name under its parent. In
  // that case, User Data at a volume's root was unsupported.
  base::FilePath parent = user_data_dir.DirName();
  if (parent != user_data_dir)
    DeleteAllRenamedUserDirectories(parent, user_data_dir.BaseName());

  if (!disk_cache_dir.empty()) {
    // Cache dir at a volume's root is unsupported.
    parent = disk_cache_dir.DirName();
    if (parent != disk_cache_dir)
      DeleteAllRenamedUserDirectories(parent, disk_cache_dir.BaseName());
  }
}

bool UserDataSnapshotEnabled() {
  return g_snapshots_enabled_for_testing ||
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
         base::IsEnterpriseDevice() ||
#endif
         policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid();
}

#if BUILDFLAG(IS_WIN)
bool IsAdministratorDrivenDowngrade(uint16_t current_milestone) {
  const auto downgrade_version = InstallUtil::GetDowngradeVersion();
  return downgrade_version &&
         downgrade_version->components()[0] > current_milestone;
}
#endif

}  // namespace

bool DowngradeManager::PrepareUserDataDirectoryForCurrentVersion(
    const base::FilePath& user_data_dir) {
  DCHECK_EQ(type_, Type::kNone);
  DCHECK(!user_data_dir.empty());

  // Do not attempt migration if this process is the product of a relaunch from
  // a previous in which migration was attempted/performed.
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUserDataMigrated)) {
    // Strip the switch from the command line so that it does not propagate to
    // any subsequent relaunches.
    command_line.RemoveSwitch(switches::kUserDataMigrated);
    return false;
  }

  std::optional<base::Version> last_version = GetLastVersion(user_data_dir);
  if (!last_version)
    return false;

  const base::Version& current_version = version_info::GetVersion();

  const bool user_data_snapshot_enabled = UserDataSnapshotEnabled();

  if (!user_data_snapshot_enabled) {
    if (current_version >= *last_version)
      return false;  // Same version or upgrade.

    type_ = GetDowngradeType(user_data_dir, current_version, *last_version);
    DCHECK(type_ == Type::kAdministrativeWipe || type_ == Type::kUnsupported);
    return type_ == Type::kAdministrativeWipe;
  }

  if (current_version == *last_version)
    return false;  // Nothing to do if the version has not changed.

  if (current_version < *last_version) {
    type_ = GetDowngradeTypeWithSnapshot(user_data_dir, current_version,
                                         *last_version);

    return type_ == Type::kAdministrativeWipe ||
           type_ == Type::kSnapshotRestore;
  }

  auto current_milestone = current_version.components()[0];
  int max_number_of_snapshots = g_browser_process->local_state()->GetInteger(
      prefs::kUserDataSnapshotRetentionLimit);
  std::optional<uint32_t> purge_milestone;
  if (current_milestone == last_version->components()[0]) {
    // Mid-milestone snapshots are only taken on canary installs.
    if (chrome::GetChannel() != version_info::Channel::CANARY)
      return false;
    // Keep one snapshot in this milestone unless snapshots are disabled.
    max_number_of_snapshots = std::min(max_number_of_snapshots, 1);
    purge_milestone = current_milestone;
  }
  SnapshotManager snapshot_manager(user_data_dir);
  snapshot_manager.TakeSnapshot(*last_version);
  snapshot_manager.PurgeInvalidAndOldSnapshots(max_number_of_snapshots,
                                               purge_milestone);
  return false;
}

void DowngradeManager::UpdateLastVersion(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  DCHECK_NE(type_, Type::kAdministrativeWipe);
  const std::string_view version(PRODUCT_VERSION);
  base::WriteFile(GetLastVersionFile(user_data_dir), version);
}

void DowngradeManager::DeleteMovedUserDataSoon(
    const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  // IWYU note: base/location.h and base/task/task_traits.h are guaranteed to be
  // available via base/task/thread_pool.h.
  content::BrowserThread::PostBestEffortTask(
      FROM_HERE,
      base::ThreadPool::CreateTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      base::BindOnce(&DeleteMovedUserData, user_data_dir, GetDiskCacheDir()));
}

void DowngradeManager::ProcessDowngrade(const base::FilePath& user_data_dir) {
  DCHECK(type_ == Type::kAdministrativeWipe || type_ == Type::kSnapshotRestore);
  DCHECK(!user_data_dir.empty());

  const base::FilePath disk_cache_dir(GetDiskCacheDir());
  if (!disk_cache_dir.empty())
    MoveCache(disk_cache_dir);

  // User Data requires special treatment, as certain files/directories should
  // be left behind. Furthermore, User Data is moved to a new directory within
  // itself (for example, to User Data/User Data.CHROME_DELETE) to guarantee
  // that the movement isn't across volumes.
  // This has a 95% success rate according to the histogram
  // "Downgrade.UserDataDirMove.Result" which is acceptable in this case since
  // the files (usually under 5 files according to
  // "Downgrade.UserDataDirMove.FailureCount") left behind 5% of the time might
  // be overridden by `SnapshotManager::RestoreSnapshot` or updated following a
  // version upgrade.
  MoveUserData(user_data_dir, GetTempDirNameForDelete(
                                  user_data_dir, user_data_dir.BaseName()));

  if (type_ == Type::kSnapshotRestore) {
    SnapshotManager snapshot_manager(user_data_dir);
    snapshot_manager.RestoreSnapshot(version_info::GetVersion());
  }

  // Add the migration switch to the command line so that it is propagated to
  // the relaunched process. This is used to prevent a relaunch bomb in case of
  // pathological failure.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUserDataMigrated);
}

// static
void DowngradeManager::EnableSnapshotsForTesting(bool enable) {
  g_snapshots_enabled_for_testing = enable;
}

// static
DowngradeManager::Type DowngradeManager::GetDowngradeType(
    const base::FilePath& user_data_dir,
    const base::Version& current_version,
    const base::Version& last_version) {
  DCHECK(!user_data_dir.empty());
  DCHECK_LT(current_version, last_version);

#if BUILDFLAG(IS_WIN)
  // Move User Data aside for a clean launch if it follows an
  // administrator-driven downgrade.
  if (IsAdministratorDrivenDowngrade(current_version.components()[0]))
    return Type::kAdministrativeWipe;
#endif
    return Type::kUnsupported;
}

// static
DowngradeManager::Type DowngradeManager::GetDowngradeTypeWithSnapshot(
    const base::FilePath& user_data_dir,
    const base::Version& current_version,
    const base::Version& last_version) {
  DCHECK(!user_data_dir.empty());
  DCHECK_LT(current_version, last_version);

  const uint16_t milestone = current_version.components()[0];

  // Move User Data and restore from a snapshot if there is a candidate
  // snapshot to restore.
  const auto snapshot_to_restore =
      GetSnapshotToRestore(current_version, user_data_dir);

#if BUILDFLAG(IS_WIN)
  // Move User Data aside for a clean launch if it follows an
  // administrator-driven downgrade when no snapshot is found.
  if (!snapshot_to_restore && IsAdministratorDrivenDowngrade(milestone))
    return Type::kAdministrativeWipe;
#endif

  const uint16_t last_milestone = last_version.components()[0];
  if (last_milestone > milestone)
    return snapshot_to_restore ? Type::kSnapshotRestore : Type::kUnsupported;

  return Type::kMinorDowngrade;
}

}  // namespace downgrade
