// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_mac.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#import "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "content/public/common/content_paths.h"

void SetUpBundleOverrides() {
  @autoreleasepool {
    base::mac::SetOverrideFrameworkBundlePath(chrome::GetFrameworkBundlePath());

    NSBundle* base_bundle = chrome::OuterAppBundle();
    base::mac::SetBaseBundleID([[base_bundle bundleIdentifier] UTF8String]);

    base::FilePath child_exe_path =
        chrome::GetFrameworkBundlePath().Append("Helpers").Append(
            chrome::kHelperProcessExecutablePath);

    // On the Mac, the child executable lives at a predefined location within
    // the app bundle's versioned directory.
    base::PathService::Override(content::CHILD_PROCESS_EXE, child_exe_path);
  }
}
