// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderIOS which replaces base::PathProviderPosix for iOS
// in base/path_service.cc.

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/base_paths.h"
#include "base/base_paths_apple.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/path_service.h"

namespace base {

bool PathProviderIOS(int key, base::FilePath* result) {
  switch (key) {
    case base::FILE_EXE:
      *result = base::apple::internal::GetExecutablePath();
      return true;
    case base::FILE_MODULE:
      return base::apple::internal::GetModulePathForAddress(
          result, reinterpret_cast<const void*>(&base::PathProviderIOS));
    case base::DIR_APP_DATA: {
      bool success =
          base::mac::GetUserDirectory(NSApplicationSupportDirectory, result);
      // On iOS, this directory does not exist unless it is created explicitly.
      if (success && !base::PathExists(*result)) {
        success = base::CreateDirectory(*result);
      }
      return success;
    }
    case base::DIR_SRC_TEST_DATA_ROOT:
      // On iOS, there is no access to source root, however, the necessary
      // resources are packaged into the test as assets.
      return PathService::Get(base::DIR_ASSETS, result);
    case base::DIR_USER_DESKTOP:
      // iOS does not have desktop directories.
      NOTIMPLEMENTED();
      return false;
    case base::DIR_ASSETS:
#if !BUILDFLAG(IS_IOS_MACCATALYST)
      // On iOS, the assets are located next to the module binary.
      return PathService::Get(base::DIR_MODULE, result);
#else
      *result = base::apple::MainBundlePath()
                    .Append(FILE_PATH_LITERAL("Contents"))
                    .Append(FILE_PATH_LITERAL("Resources"));
      return true;
#endif  // !BUILDFLAG(IS_IOS_MACCATALYST)
    case base::DIR_CACHE:
      return base::mac::GetUserDirectory(NSCachesDirectory, result);
    default:
      return false;
  }
}

}  // namespace base
