// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_PATHS_H_
#define BASE_BASE_PATHS_H_

// This file declares path keys for the base module.  These can be used with
// the PathService to access various special directories and files.

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#elif BUILDFLAG(IS_MAC)
#include "base/base_paths_mac.h"
#elif BUILDFLAG(IS_IOS)
#include "base/base_paths_ios.h"
#elif BUILDFLAG(IS_ANDROID)
#include "base/base_paths_android.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/base_paths_posix.h"
#endif

namespace base {

enum BasePathKey {
  PATH_START = 0,

  // The following refer to the current application.
  FILE_EXE,  // Path and filename of the current executable.
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  // Prefer keys (e.g., DIR_ASSETS) that are specific to the use case as the
  // module location may not work as expected on some platforms. For this
  // reason, this key is not defined on Fuchsia. See crbug.com/1263691 for
  // details.
  FILE_MODULE,  // Path and filename of the module containing the code for
                // the PathService (which could differ from FILE_EXE if the
                // PathService were compiled into a shared object, for
                // example).
#endif
  DIR_EXE,  // Directory containing FILE_EXE.
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_IOS)
  // Prefer keys (e.g., DIR_ASSETS) that are specific to the use case as the
  // module location may not work as expected on some platforms. For this
  // reason, this key is not defined on Fuchsia. See crbug.com/1263691 for
  // details.
  DIR_MODULE,  // Directory containing FILE_MODULE.
#endif
  DIR_ASSETS,  // Directory that contains application assets.

  // The following refer to system and system user directories.
  DIR_TEMP,          // Temporary directory for the system and/or user.
  DIR_HOME,          // User's root home directory. On Windows this will look
                     // like "C:\Users\<user>"  which isn't necessarily a great
                     // place to put files.
#if !BUILDFLAG(IS_IOS)
  DIR_USER_DESKTOP,  // The current user's Desktop.
#endif

  // The following refer to the applications current environment.
  DIR_CURRENT,  // Current directory.

  // The following are only for use in tests.
  // On some platforms, such as Android and Fuchsia, tests do not have access to
  // the build file system so the necessary files are bundled with the test
  // binary. On such platforms, these will return an appropriate path inside the
  // bundle.
  DIR_SRC_TEST_DATA_ROOT,  // The root of files in the source tree that are
                           // made available to tests. Useful for tests that use
                           // resources that exist in the source tree.
  DIR_SOURCE_ROOT = DIR_SRC_TEST_DATA_ROOT,  // Legacy name still widely used.
                                             // TODO(crbug.com/1264897): Replace
                                             // all instances and remove alias.
  DIR_GEN_TEST_DATA_ROOT,  // The root of generated files that are
                           // made available to tests. Note: for build-outputs
                           // needed by tests, DIR_ASSETS should be used.
  DIR_TEST_DATA,           // Directory containing test data for //base tests.
                           // Only for use in base_unittests. Equivalent to
                           // DIR_SRC_TEST_DATA_ROOT + "/base/test/data".

  PATH_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_H_
