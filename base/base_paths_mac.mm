// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderMac which replaces base::PathProviderPosix for Mac
// in base/path_service.cc.

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/base_paths_apple.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"

namespace base {

bool PathProviderMac(int key, base::FilePath* result) {
  switch (key) {
    case base::FILE_EXE:
      *result = base::apple::internal::GetExecutablePath();
      return true;
    case base::FILE_MODULE:
      return base::apple::internal::GetModulePathForAddress(
          result, reinterpret_cast<const void*>(&base::PathProviderMac));
    case base::DIR_APP_DATA: {
      bool success =
          base::apple::GetUserDirectory(NSApplicationSupportDirectory, result);
      return success;
    }
    case base::DIR_SRC_TEST_DATA_ROOT:
      // Go through PathService to catch overrides.
      if (!PathService::Get(base::FILE_EXE, result)) {
        return false;
      }

      // Start with the executable's directory.
      *result = result->DirName();

      if (base::apple::AmIBundled()) {
        // The bundled app executables (Chromium, TestShell, etc) live five
        // levels down, eg:
        // src/xcodebuild/{Debug|Release}/Chromium.app/Contents/MacOS/Chromium
        *result = result->DirName().DirName().DirName().DirName().DirName();
      } else {
        // Unit tests execute two levels deep from the source root, eg:
        // src/xcodebuild/{Debug|Release}/base_unittests
        *result = result->DirName().DirName();
      }
      return true;
    case base::DIR_USER_DESKTOP:
      return base::apple::GetUserDirectory(NSDesktopDirectory, result);
    case base::DIR_ASSETS:
      if (!base::apple::AmIBundled()) {
        return PathService::Get(base::DIR_MODULE, result);
      }
      *result = base::apple::FrameworkBundlePath().Append(
          FILE_PATH_LITERAL("Resources"));
      return true;
    case base::DIR_CACHE:
      return base::apple::GetUserDirectory(NSCachesDirectory, result);
    default:
      return false;
  }
}

}  // namespace base
