// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/stl_util.h"
#include "base/system/sys_info.h"

namespace {

// Return a 3 elements array containing the major, minor and bug fix version of
// the OS.
const int32_t* OSVersionAsArray() {
  int32_t* digits = new int32_t[3];
  base::SysInfo::OperatingSystemVersionNumbers(
      &digits[0], &digits[1], &digits[2]);
  return digits;
}

std::string* g_icudtl_path_override = nullptr;

}  // namespace

namespace base {
namespace ios {

bool IsRunningOnIOS10OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(10, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS11OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(11, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS12OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(12, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS13OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(13, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnOrLater(int32_t major, int32_t minor, int32_t bug_fix) {
  static const int32_t* current_version = OSVersionAsArray();
  int32_t version[] = {major, minor, bug_fix};
  for (size_t i = 0; i < base::size(version); i++) {
    if (current_version[i] != version[i])
      return current_version[i] > version[i];
  }
  return true;
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

}  // namespace ios
}  // namespace base
