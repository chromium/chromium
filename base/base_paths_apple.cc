// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths_apple.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <stdint.h>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace base::apple::internal {

base::FilePath GetExecutablePath() {
  // Executable path can have relative references ("..") depending on
  // how the app was launched.
  uint32_t executable_length = 0;
  _NSGetExecutablePath(NULL, &executable_length);
  DCHECK_GT(executable_length, 1u);
  std::string executable_path;
  int rv = _NSGetExecutablePath(
      base::WriteInto(&executable_path, executable_length), &executable_length);
  DCHECK_EQ(rv, 0);

  // _NSGetExecutablePath may return paths containing ./ or ../ which makes
  // FilePath::DirName() work incorrectly, convert it to absolute path so that
  // paths such as DIR_SRC_TEST_DATA_ROOT can work, since we expect absolute
  // paths to be returned here.
  // TODO(bauerb): http://crbug.com/259796, http://crbug.com/373477
  base::ScopedAllowBlocking allow_blocking;
  return base::MakeAbsoluteFilePath(base::FilePath(executable_path));
}

bool GetModulePathForAddress(base::FilePath* path, const void* address) {
  Dl_info info;
  if (dladdr(address, &info) == 0) {
    return false;
  }
  *path = base::FilePath(info.dli_fname);
  return true;
}

}  // namespace base::apple::internal
