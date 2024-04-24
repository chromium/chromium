// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/ios/ios_util.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <stddef.h>

#include "base/apple/foundation_util.h"
#include "base/system/sys_info.h"

namespace {

std::string* g_icudtl_path_override = nullptr;

}  // namespace

namespace base::ios {

bool IsRunningOnIOS16OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(16, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS17OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(17, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnOrLater(int32_t major, int32_t minor, int32_t bug_fix) {
  static const class OSVersion {
   public:
    OSVersion() {
      SysInfo::OperatingSystemVersionNumbers(
          &current_version_[0], &current_version_[1], &current_version_[2]);
    }

    bool IsRunningOnOrLater(int32_t version[3]) const {
      for (size_t i = 0; i < std::size(current_version_); ++i) {
        if (current_version_[i] != version[i])
          return current_version_[i] > version[i];
      }
      return true;
    }

   private:
    int32_t current_version_[3];
  } kOSVersion;

  int32_t version[3] = {major, minor, bug_fix};
  return kOSVersion.IsRunningOnOrLater(version);
}

bool IsInForcedRTL() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  return [defaults boolForKey:@"NSForceRightToLeftWritingDirection"];
}

void OverridePathOfEmbeddedICU(const char* path) {
  DCHECK(!g_icudtl_path_override);
  g_icudtl_path_override = new std::string(path);
}

FilePath FilePathOfEmbeddedICU() {
  if (g_icudtl_path_override) {
    return FilePath(*g_icudtl_path_override);
  }
  return FilePath();
}

#if !BUILDFLAG(IS_IOS_APP_EXTENSION)
bool IsMultipleScenesSupported() {
  if (@available(iOS 13, *)) {
    return UIApplication.sharedApplication.supportsMultipleScenes;
  }
  return false;
}
#endif

bool IsApplicationPreWarmed() {
  return [NSProcessInfo.processInfo.environment objectForKey:@"ActivePrewarm"];
}

}  // namespace base::ios
