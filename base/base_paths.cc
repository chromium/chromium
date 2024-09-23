// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base {

// This provider aims at overriding the initial behaviour for all platforms. It
// is meant to be run **before** the platform specific provider so that this one
// prevails in case the overriding conditions are met. This provider is also
// meant to fallback on the platform specific provider, which means it should
// not handle the `BasePathKey` for which we do not have overriding behaviours.
bool EnvOverridePathProvider(int key, FilePath* result) {
  switch (key) {
    case base::DIR_SRC_TEST_DATA_ROOT: {
      // Allow passing this in the environment, for more flexibility in build
      // tree configurations (sub-project builds, gyp --output_dir, etc.)
      std::unique_ptr<Environment> env(Environment::Create());
      std::string cr_source_root;
      FilePath path;
      if (env->GetVar("CR_SOURCE_ROOT", &cr_source_root)) {
#if BUILDFLAG(IS_WIN)
        path = FilePath(UTF8ToWide(cr_source_root));
#else
        path = FilePath(cr_source_root);
#endif
        if (!path.IsAbsolute()) {
          FilePath root;
          if (PathService::Get(DIR_EXE, &root)) {
            path = root.Append(path);
          }
        }
        if (DirectoryExists(path)) {
          *result = path;
          return true;
        }
        DLOG(WARNING) << "CR_SOURCE_ROOT is set, but it appears to not "
                      << "point to a directory.";
      }
      return false;
    }
    default:
      break;
  }
  return false;
}

bool PathProvider(int key, FilePath* result) {
  // NOTE: DIR_CURRENT is a special case in PathService::Get

  switch (key) {
    case DIR_EXE:
      if (!PathService::Get(FILE_EXE, result))
        return false;
      *result = result->DirName();
      return true;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case DIR_MODULE:
      if (!PathService::Get(FILE_MODULE, result))
        return false;
      *result = result->DirName();
      return true;
    case DIR_ASSETS:
      return PathService::Get(DIR_MODULE, result);
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case DIR_TEMP:
      return GetTempDir(result);
    case DIR_HOME:
      *result = GetHomeDir();
      return true;
    case base::DIR_SRC_TEST_DATA_ROOT:
      // This is only used by tests and overridden by each platform.
      NOTREACHED();
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
    case DIR_OUT_TEST_DATA_ROOT:
      // On most platforms test binaries are run directly from the build-output
      // directory, so return the directory containing the executable.
      return PathService::Get(DIR_MODULE, result);
#endif  // !BUILDFLAG(IS_FUCHSIA)  && !BUILDFLAG(IS_IOS)
    case DIR_GEN_TEST_DATA_ROOT:
      if (!PathService::Get(DIR_OUT_TEST_DATA_ROOT, result)) {
        return false;
      }
      *result = result->Append(FILE_PATH_LITERAL("gen"));
      return true;
    case DIR_TEST_DATA: {
      FilePath test_data_path;
      if (!PathService::Get(DIR_SRC_TEST_DATA_ROOT, &test_data_path)) {
        return false;
      }
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
