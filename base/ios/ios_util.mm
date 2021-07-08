// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <stddef.h>

#include "base/cxx17_backports.h"
#include "base/mac/foundation_util.h"
#include "base/system/sys_info.h"

namespace {

// Key for the scene API manifest in the application Info.plist.
NSString* const kApplicationSceneManifestKey = @"UIApplicationSceneManifest";

// Key for the boolean telling whether the multi-scene support is enabled for
// the application in the scene API manifest.
NSString* const kApplicationSupportsMultipleScenesKey =
    @"UIApplicationSupportsMultipleScenes";

// Return a 3 elements array containing the major, minor and bug fix version of
// the OS.
const int32_t* OSVersionAsArray() {
  int32_t* digits = new int32_t[3];
  base::SysInfo::OperatingSystemVersionNumbers(
      &digits[0], &digits[1], &digits[2]);
  return digits;
}

// Return an autoreleased pointer to the dictionary configuring the scene API
// from the application Info.plist. Can be null if the application or the OS
// version does not use the scene API.
NSDictionary* SceneAPIManifestFromInfoPlist() {
  // Scene API is only supported on iOS 13.0+.
  if (!base::ios::IsRunningOnIOS13OrLater())
    return nil;

  NSBundle* main_bundle = [NSBundle mainBundle];
  return base::mac::ObjCCastStrict<NSDictionary>(
      [main_bundle objectForInfoDictionaryKey:kApplicationSceneManifestKey]);
}

std::string* g_icudtl_path_override = nullptr;

}  // namespace

namespace base {
namespace ios {

bool IsRunningOnIOS12OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(12, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS13OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(13, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS14OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(14, 0, 0);
  return is_running_on_or_later;
}

bool IsRunningOnIOS15OrLater() {
  static const bool is_running_on_or_later = IsRunningOnOrLater(15, 0, 0);
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

bool IsMultiwindowSupported() {
  static bool cached_value = false;
  static dispatch_once_t once_token = 0;
  dispatch_once(&once_token, ^{
    NSDictionary* scene_api_manifest = SceneAPIManifestFromInfoPlist();
    NSNumber* value = base::mac::ObjCCastStrict<NSNumber>([scene_api_manifest
        objectForKey:kApplicationSupportsMultipleScenesKey]);
    cached_value = [value boolValue];
  });
  return cached_value;
}

bool IsSceneStartupSupported() {
  static bool cached_value = false;
  static dispatch_once_t once_token = 0;
  dispatch_once(&once_token, ^{
    NSDictionary* scene_api_manifest = SceneAPIManifestFromInfoPlist();
    cached_value = scene_api_manifest != nil;
  });
  return cached_value;
}

bool IsMultipleScenesSupported() {
  if (@available(iOS 13, *)) {
    return UIApplication.sharedApplication.supportsMultipleScenes;
  }
  return false;
}

}  // namespace ios
}  // namespace base
