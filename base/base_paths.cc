// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"

namespace base {

bool PathProvider(int key, FilePath* result) {
  // NOTE: DIR_CURRENT is a special case in PathService::Get

  switch (key) {
    case DIR_EXE:
      if (!PathService::Get(FILE_EXE, result))
        return false;
      *result = result->DirName();
      return true;
#if !BUILDFLAG(IS_FUCHSIA)
    case DIR_MODULE:
      if (!PathService::Get(FILE_MODULE, result))
        return false;
      *result = result->DirName();
      return true;
    case DIR_ASSETS:
      return PathService::Get(DIR_MODULE, result);
#endif  // !BUILDFLAG(IS_FUCHSIA)
    case DIR_TEMP:
      return GetTempDir(result);
    case DIR_HOME:
      *result = GetHomeDir();
      return true;
    case DIR_GEN_TEST_DATA_ROOT:
#if !BUILDFLAG(IS_FUCHSIA)
      // On most platforms, all build output is in the same directory, so
      // use DIR_MODULE to get the path to the current binary.
      return PathService::Get(DIR_MODULE, result);
#endif  // !BUILDFLAG(IS_FUCHSIA)
    case DIR_TEST_DATA: {
      FilePath test_data_path;
      if (!PathService::Get(DIR_SRC_TEST_DATA_ROOT, &test_data_path))
        return false;
      test_data_path = test_data_path.Append(FILE_PATH_LITERAL("base"));
      test_data_path = test_data_path.Append(FILE_PATH_LITERAL("test"));
      test_data_path = test_data_path.Append(FILE_PATH_LITERAL("data"));
      if (!PathExists(test_data_path))  // We don't want to create this.
        return false;
      *result = test_data_path;
      return true;
    }
  }

  return false;
}

}  // namespace base
