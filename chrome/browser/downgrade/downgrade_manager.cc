// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/downgrade_manager.h"

#include <windows.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/browser/browser_thread.h"

namespace downgrade {

namespace {

// Returns true if User Data should be moved aside for a downgrade.
bool ShouldMoveUserData(const base::Version& current_version) {
  // Move User Data only if it follows an administrator-driven downgrade.
  return InstallUtil::GetDowngradeVersion() > current_version;
}

// Returns a unique name for a path of the form |dir|/|name|.CHROME_DELETE, or
// an empty path if none such can be found. The path may contain " (N)" with
// some integer N before the final file extension.
base::FilePath GetTempDirNameForDelete(const base::FilePath& dir,
                                       const base::FilePath& name) {
  if (dir.empty())
    return base::FilePath();

  return base::GetUniquePath(
      dir.Append(name).AddExtension(kDowngradeDeleteSuffix));
}

// Attempts to move/rename |source| to |target| without falling back to
// copy-and-delete. Returns true on success.
bool MoveWithoutFallback(const base::FilePath& source,
                         const base::FilePath& target) {
  // TODO(grt): check whether or not this is sufficiently atomic when |source|
  // is on a network share.
  auto result = ::MoveFileEx(base::as_wcstr(source.value()),
                             base::as_wcstr(target.value()), 0);
  PLOG_IF(ERROR, !result) << source << " -> " << target;
  return result;
}

// A callback that returns true when its argument names a path that should not
// be moved by MoveContents.
using ExclusionPredicate = base::RepeatingCallback<bool(const base::FilePath&)>;

// Moves the contents of directory |source| into the directory |target| (which
// may or may not exist) for deletion at a later time. Any directories that
// cannot be moved (most likely due to open files therein) are recursed into.
// |exclusions_predicate| is an optional callback that evaluates items in
// |source| to determine whether or not they should be skipped. Returns the
// number of items within |source| or its subdirectories that could not be
// moved, or no value if |target| could not be created.
base::Optional<int> MoveContents(const base::FilePath& source,
                                 const base::FilePath& target,
                                 ExclusionPredicate exclusion_predicate) {
  // Implementation note: moving is better than deleting in this case since it
  // avoids certain failure modes. For example: on Windows, a file that is open
  // with FILE_SHARE_DELETE can be moved or marked for deletion. If it is moved
  // aside, the containing directory may then be eligible for deletion. If, on
  // the other hand, it is marked for deletion, it cannot be moved nor can its
  // containing directory be moved or deleted.
  if (!base::CreateDirectory(target)) {
    PLOG(ERROR) << target;
    return base::nullopt;
  }

  int failure_count = 0;
  base::FileEnumerator enumerator(
      source, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    const base::FilePath name = info.GetName();
    if (exclusion_predicate && exclusion_predicate.Run(name))
      continue;
    const base::FilePath this_target = target.Append(name);
    // A directory can be moved unless any file within it is open. A simple file
    // can be moved unless it is opened without FILE_SHARE_DELETE. (As with most
    // things in life, there are exceptions to this rule, but they are
    // uncommon. For example, a file opened without FILE_SHARE_DELETE can be
    // moved as long as it was opened only with some combination of
    // READ_CONTROL, WRITE_DAC, WRITE_OWNER, and SYNCHRONIZE access rights.
    // Since this short list excludes such useful rights as FILE_EXECUTE,
    // FILE_READ_DATA, and most anything else one would want a file for, it's
    // likely an uncommon scenario. See OpenFileTest in base/files for more.)
    if (MoveWithoutFallback(path, this_target))
      continue;
    if (!info.IsDirectory()) {
      ++failure_count;
      // TODO(grt): Consider if UKM can be used to learn the relative path of
      // file(s) that cannot be moved.
      continue;
    }
    failure_count +=
        MoveContents(path, this_target, ExclusionPredicate()).value_or(0);
    // If everything within the directory was moved, it may be possible to
    // delete it now.
    if (!base::DeleteFile(path, false /* !recursive */))
      ++failure_count;
  }
  return failure_count;
}

// Moves the contents of a User Data directory at |source| to |target|, with the
// exception of files/directories that should be left behind for a full data
// wipe. Returns no value if the target directory could not be created, or the
// number of items that could not be moved.
base::Optional<int> MoveUserData(const base::FilePath& source,
                                 const base::FilePath& target) {
  // Returns true to exclude a file.
  auto exclusion_predicate =
      base::BindRepeating([](const base::FilePath& name) -> bool {
        static constexpr base::FilePath::StringPieceType kFilesToKeep[] = {
            FILE_PATH_LITERAL("browsermetrics"),
            FILE_PATH_LITERAL("crashpad"),
            FILE_PATH_LITERAL("first run"),
            FILE_PATH_LITERAL("last version"),
            FILE_PATH_LITERAL("lockfile"),
            FILE_PATH_LITERAL("stability"),
        };
        // Don't try to move the dir into which everything is being moved.
        if (name.FinalExtension() == kDowngradeDeleteSuffix)
          return true;
        return std::find_if(std::begin(kFilesToKeep), std::end(kFilesToKeep),
                            [&name](const auto& keep) {
                              return base::EqualsCaseInsensitiveASCII(
                                  name.value(), keep);
                            }) != std::end(kFilesToKeep);
      });
  auto result = MoveContents(source, target, std::move(exclusion_predicate));

  // Move the Last Version file last so that any crash before this point results
  // in a retry on the next launch.
  if (!result ||
      !MoveWithoutFallback(source.Append(kDowngradeLastVersionFile),
                           target.Append(kDowngradeLastVersionFile))) {
    if (result)
      *result += 1;
    // Attempt to delete Last Version if all else failed so that Chrome does not
    // continually attempt to perform a migration.
    base::DeleteFile(source.Append(kDowngradeLastVersionFile),
                     false /* recursive */);
    // Inform system administrators that things have gone awry.
    SYSLOG(ERROR) << "Failed to perform User Data migration following a Chrome "
                     "version downgrade. Chrome will run with User Data from a "
                     "higher version and may behave unpredictably.";
    // At this point, Chrome will relaunch with --user-data-migrated. This
    // switch suppresses downgrade processing, so that launch will go through
    // normal startup.
  }
  return result;
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

  // The cache dir should have no files in use, so a simple move should suffice.
  const bool move_result = MoveWithoutFallback(disk_cache_dir, target);
  base::UmaHistogramBoolean("Downgrade.CacheDirMove.Result", move_result);
  if (move_result)
    return;

  // The directory couldn't be moved whole-hog. Attempt a recursive move of its
  // contents.
  auto failure_count =
      MoveContents(disk_cache_dir, target, ExclusionPredicate());
  if (!failure_count || *failure_count) {
    // Report precise values rather than an exponentially bucketed histogram.
    // Bucket 0 means that the target directory could not be created. All other
    // buckets are a count of files/directories left behind.
    base::UmaHistogramExactLinear("Downgrade.CacheDirMove.FailureCount",
                                  failure_count.value_or(0), 50);
  }
}

// Deletes all subdirectories in |dir| named |name|*.CHROME_DELETE.
void DeleteAllRenamedUserDirectories(const base::FilePath& dir,
                                     const base::FilePath& name) {
  base::FilePath::StringType pattern = name.value() + FILE_PATH_LITERAL("*");
  kDowngradeDeleteSuffix.AppendToString(&pattern);
  base::FileEnumerator enumerator(dir, false, base::FileEnumerator::DIRECTORIES,
                                  pattern);
  for (base::FilePath to_delete = enumerator.Next(); !to_delete.empty();
       to_delete = enumerator.Next()) {
    base::DeleteFile(to_delete, true /* recursive */);
  }
}

// Deletes all moved User Data and Cache directories for the given dirs.
void DeleteMovedUserData(const base::FilePath& user_data_dir,
                         const base::FilePath& disk_cache_dir) {
  DeleteAllRenamedUserDirectories(user_data_dir, user_data_dir.BaseName());

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

}  // namespace

bool DowngradeManager::IsMigrationRequired(
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

  base::Optional<base::Version> last_version = GetLastVersion(user_data_dir);
  if (!last_version)
    return false;

  const base::Version current_version(chrome::kChromeVersion);
  if (current_version >= *last_version)
    return false;  // Same version or upgrade.

  type_ = ShouldMoveUserData(current_version) ? Type::kAdministrativeWipe
                                              : Type::kUnsupported;
  base::UmaHistogramEnumeration("Downgrade.Type", type_);
  return type_ == Type::kAdministrativeWipe;
}

void DowngradeManager::UpdateLastVersion(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  DCHECK_NE(type_, Type::kAdministrativeWipe);
  const base::StringPiece version(chrome::kChromeVersion);
  base::WriteFile(GetLastVersionFile(user_data_dir), version.data(),
                  version.size());
}

void DowngradeManager::DeleteMovedUserDataSoon(
    const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  // IWYU note: base/location.h and base/task/task_traits.h are guaranteed to be
  // available via base/task/post_task.h.
  content::BrowserThread::PostBestEffortTask(
      FROM_HERE,
      base::CreateTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      base::BindOnce(&DeleteMovedUserData, user_data_dir, GetDiskCacheDir()));
}

void DowngradeManager::ProcessDowngrade(const base::FilePath& user_data_dir) {
  DCHECK_EQ(type_, Type::kAdministrativeWipe);
  DCHECK(!user_data_dir.empty());

  const base::FilePath disk_cache_dir(GetDiskCacheDir());
  if (!disk_cache_dir.empty())
    MoveCache(disk_cache_dir);

  // User Data requires special treatment, as certain files/directories should
  // be left behind. Furthermore, User Data is moved to a new directory within
  // itself (for example, to User Data/User Data.CHROME_DELETE) to guarantee
  // that the movement isn't across volumes.
  const auto failure_count = MoveUserData(
      user_data_dir,
      GetTempDirNameForDelete(user_data_dir, user_data_dir.BaseName()));
  enum class UserDataMoveResult {
    kCreateTargetFailure = 0,
    kSuccess = 1,
    kPartialSuccess = 2,
    kMaxValue = kPartialSuccess
  };
  UserDataMoveResult move_result =
      !failure_count ? UserDataMoveResult::kCreateTargetFailure
                     : (*failure_count ? UserDataMoveResult::kPartialSuccess
                                       : UserDataMoveResult::kSuccess);
  base::UmaHistogramEnumeration("Downgrade.UserDataDirMove.Result",
                                move_result);
  if (failure_count && *failure_count) {
    // Report precise values rather than an exponentially bucketed histogram.
    base::UmaHistogramExactLinear("Downgrade.UserDataDirMove.FailureCount",
                                  *failure_count, 50);
  }

  // Add the migration switch to the command line so that it is propagated to
  // the relaunched process. This is used to prevent a relaunch bomb in case of
  // pathological failure.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kUserDataMigrated);
}

}  // namespace downgrade
