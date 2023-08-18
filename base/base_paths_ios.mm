// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines base::PathProviderIOS which replaces base::PathProviderPosix for iOS
// in base/path_service.cc.

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/base_paths.h"
#include "base/base_paths_apple.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace base {

bool PathProviderIOS(int key, base::FilePath* result) {
  switch (key) {
    case base::FILE_EXE:
      *result = base::apple::internal::GetExecutablePath();
      return true;

    case base::DIR_APP_DATA: {
      base::FilePath path;
      if (!base::apple::GetUserDirectory(NSApplicationSupportDirectory,
                                         &path)) {
        return false;
      }

      // On iOS, this directory does not exist unless it is created explicitly.
      if (!base::PathExists(path) && !base::CreateDirectory(path)) {
        return false;
      }

      *result = path;
      return true;
    }

    case base::DIR_SRC_TEST_DATA_ROOT:
    case base::DIR_OUT_TEST_DATA_ROOT:
      // On iOS, there is no access to source root, nor build dir,
      // however, the necessary resources are packaged into the
      // test app as assets.
      [[fallthrough]];

    case base::DIR_ASSETS:
      // On iOS, the resources are located at the root of the framework bundle.
      *result = base::apple::FrameworkBundlePath();
#if BUILDFLAG(IS_IOS_MACCATALYST)
      // When running in the catalyst environment (i.e. building an iOS app
      // to run on macOS), the bundles have the same structure as macOS, so
      // the resources are in the "Contents/Resources" sub-directory.
      *result = result->Append(FILE_PATH_LITERAL("Contents"))
                    .Append(FILE_PATH_LITERAL("Resources"));
#endif  // BUILDFLAG(IS_IOS_MACCATALYST)
      return true;

    case base::DIR_CACHE:
      return base::apple::GetUserDirectory(NSCachesDirectory, result);

    default:
      return false;
  }
}

}  // namespace base
