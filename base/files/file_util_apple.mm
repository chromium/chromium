// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#import <Foundation/Foundation.h>
#include <copyfile.h>
#include <stdlib.h>
#include <string.h>

#include "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/threading/scoped_blocking_call.h"

namespace base {

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  if (from_path.ReferencesParent() || to_path.ReferencesParent()) {
    return false;
  }
  return (copyfile(from_path.value().c_str(), to_path.value().c_str(),
                   /*state=*/nullptr, COPYFILE_DATA) == 0);
}

bool GetTempDir(base::FilePath* path) {
  // In order to facilitate hermetic runs on macOS, first check
  // MAC_CHROMIUM_TMPDIR. This is used instead of TMPDIR for historical reasons.
  // This was originally done for https://crbug.com/698759 (TMPDIR too long for
  // process singleton socket path), but is hopefully obsolete as of
  // https://crbug.com/1266817 (allows a longer process singleton socket path).
  // Continue tracking MAC_CHROMIUM_TMPDIR as that's what build infrastructure
  // sets on macOS.
  const char* env_tmpdir = getenv(base::env_vars::kMacChromiumTmpDir);
  if (env_tmpdir && env_tmpdir[0]) {
    *path = base::FilePath(env_tmpdir);
    return true;
  }

  // If we didn't find it, fall back to the native function.
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil) {
    return false;
  }
  *path = base::apple::NSStringToFilePath(tmp);
  return true;
}

FilePath GetHomeDir() {
  NSString* tmp = NSHomeDirectory();
  if (tmp != nil) {
    FilePath mac_home_dir = base::apple::NSStringToFilePath(tmp);
    if (!mac_home_dir.empty()) {
      return mac_home_dir;
    }
  }

  // Fall back on temp dir if no home directory is defined.
  FilePath rv;
  if (GetTempDir(&rv)) {
    return rv;
  }

  // Last resort.
  return FilePath("/tmp");
}

#if BUILDFLAG(IS_MAC)
FilePath GetDarwinUserDirectory(DarwinUserDirectory directory) {
  int confstr_name;
  switch (directory) {
    case DarwinUserDirectory::kUser:
      confstr_name = _CS_DARWIN_USER_DIR;
      break;
    case DarwinUserDirectory::kUserCache:
      confstr_name = _CS_DARWIN_USER_CACHE_DIR;
      break;
    case DarwinUserDirectory::kUserTemp:
      confstr_name = _CS_DARWIN_USER_TEMP_DIR;
      break;
    default:
      NOTREACHED();
  }

  char path_buf[PATH_MAX];
  size_t rv = confstr(confstr_name, path_buf, sizeof(path_buf));
  if (rv == 0 || rv > sizeof(path_buf)) {
    return FilePath();
  }

  // confstr() returns paths starting with `/var`, which is a symbolic link
  // to `/private/var` on macOS. File read and write permissions granted to
  // sandboxed processes do not respect non-canonical paths, so resolve symbolic
  // links to return the canonicalized path.
  return MakeAbsoluteFilePath(FilePath(path_buf));
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace base
