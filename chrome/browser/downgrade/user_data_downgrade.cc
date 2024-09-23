// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/user_data_downgrade.h"

#include <string>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/downgrade/snapshot_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace downgrade {

namespace {

base::Version GetVersionFromFileName(const base::FilePath& path) {
#if BUILDFLAG(IS_WIN)
  // On Windows, for Unicode-aware applications, native pathnames are wchar_t
  // arrays encoded in UTF-16.
  return base::Version(base::WideToUTF8(path.BaseName().value()));
#elif BUILDFLAG(IS_POSIX)
  // On most platforms, native pathnames are char arrays, and the encoding
  // may or may not be specified.  On Mac OS X, native pathnames are encoded
  // in UTF-8.
  return base::Version(path.BaseName().value());
#endif  // BUILDFLAG(IS_WIN)
}

bool IsValidSnapshotDirectory(const base::FilePath& path) {
  base::Version snapshot_version = GetVersionFromFileName(path);
  return snapshot_version.IsValid() &&
         base::PathExists(path.Append(kDowngradeLastVersionFile));
}

}  // namespace

const base::FilePath::StringPieceType kDowngradeLastVersionFile(
    FILE_PATH_LITERAL("Last Version"));
const base::FilePath::StringPieceType kDowngradeDeleteSuffix(
    FILE_PATH_LITERAL(".CHROME_DELETE"));

const base::FilePath::StringPieceType kSnapshotsDir(
    FILE_PATH_LITERAL("Snapshots"));

base::FilePath GetLastVersionFile(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  return user_data_dir.Append(kDowngradeLastVersionFile);
}

std::optional<base::Version> GetLastVersion(
    const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  std::string last_version_str;
  if (base::ReadFileToString(GetLastVersionFile(user_data_dir),
                             &last_version_str)) {
    base::Version version(
        base::TrimWhitespaceASCII(last_version_str, base::TRIM_ALL));
    if (version.IsValid())
      return version;
  }
  return std::nullopt;
}

base::FilePath GetDiskCacheDir() {
  base::FilePath disk_cache_dir =
      g_browser_process->local_state()->GetFilePath(prefs::kDiskCacheDir);
  if (disk_cache_dir.ReferencesParent())
    return base::MakeAbsoluteFilePath(disk_cache_dir);
  return disk_cache_dir;
}

base::flat_set<base::Version> GetAvailableSnapshots(
    const base::FilePath& snapshot_dir) {
  std::vector<base::Version> result;
  base::FileEnumerator enumerator(snapshot_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    base::Version snapshot_version = GetVersionFromFileName(path);
    if (!snapshot_version.IsValid() ||
        !base::PathExists(path.Append(kDowngradeLastVersionFile))) {
      continue;
    }
    result.push_back(std::move(snapshot_version));
  }
  return base::flat_set<base::Version>(std::move(result));
}

std::vector<base::FilePath> GetInvalidSnapshots(
    const base::FilePath& snapshot_dir) {
  std::vector<base::FilePath> result;
  base::FileEnumerator enumerator(snapshot_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (!IsValidSnapshotDirectory(path))
      result.push_back(std::move(path));
  }
  return result;
}

std::optional<base::Version> GetSnapshotToRestore(
    const base::Version& version,
    const base::FilePath& user_data_dir) {
  DCHECK(version.IsValid());
  base::FilePath top_snapshot_dir = user_data_dir.Append(kSnapshotsDir);
  auto available_snapshots = GetAvailableSnapshots(top_snapshot_dir);

  auto upper_bound = available_snapshots.upper_bound(version);
  if (upper_bound != available_snapshots.begin())
    return *--upper_bound;
  return std::nullopt;
}

void RemoveDataForProfile(base::Time delete_begin,
                          const base::FilePath& profile_path,
                          uint64_t remove_mask) {
  SnapshotManager snapshot_manager(profile_path.DirName());
  snapshot_manager.DeleteSnapshotDataForProfile(
      delete_begin, profile_path.BaseName(), remove_mask);
}

}  // namespace downgrade
