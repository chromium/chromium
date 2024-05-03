// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/downgrade_utils.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_POSIX)
#include "base/files/file.h"
#endif

namespace downgrade {

base::FilePath GetTempDirNameForDelete(const base::FilePath& dir,
                                       const base::FilePath& name) {
  if (dir.empty())
    return base::FilePath();

  return base::GetUniquePath(
      dir.Append(name).AddExtension(kDowngradeDeleteSuffix));
}

bool MoveWithoutFallback(const base::FilePath& source,
                         const base::FilePath& target) {
#if BUILDFLAG(IS_WIN)
  // TODO(grt): check whether or not this is sufficiently atomic when |source|
  // is on a network share.
  auto result = ::MoveFileEx(source.value().c_str(), target.value().c_str(), 0);
  PLOG_IF(ERROR, !result) << source << " -> " << target;
  return result;
#elif BUILDFLAG(IS_POSIX)
  // Windows compatibility: if |target| exists, |source| and |target|
  // must be the same type, either both files, or both directories.
  base::stat_wrapper_t target_info;
  if (base::File::Stat(target, &target_info) == 0) {
    base::stat_wrapper_t source_info;
    if (base::File::Stat(source, &source_info) != 0) {
      return false;
    }
    if (S_ISDIR(target_info.st_mode) != S_ISDIR(source_info.st_mode))
      return false;
  }

  auto result = rename(source.value().c_str(), target.value().c_str());
  PLOG_IF(ERROR, result) << source << " -> " << target;
  return !result;
#endif
}

bool MoveContents(const base::FilePath& source,
                  const base::FilePath& target,
                  ExclusionPredicate exclusion_predicate) {
  // Implementation note: moving is better than deleting in this case since it
  // avoids certain failure modes. For example: on Windows, a file that is open
  // with FILE_SHARE_DELETE can be moved or marked for deletion. If it is moved
  // aside, the containing directory may then be eligible for deletion. If, on
  // the other hand, it is marked for deletion, it cannot be moved nor can its
  // containing directory be moved or deleted.
  bool all_succeeded = base::CreateDirectory(target);
  if (!all_succeeded) {
    PLOG(ERROR) << target;
    return all_succeeded;
  }

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
      all_succeeded = false;
      continue;
    }
    MoveContents(path, this_target, ExclusionPredicate());
    // If everything within the directory was moved, it may be possible to
    // delete it now.
    if (!base::DeleteFile(path))
      all_succeeded = false;
  }
  return all_succeeded;
}

}  // namespace downgrade
