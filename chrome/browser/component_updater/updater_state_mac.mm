// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/updater_state.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/version.h"

namespace component_updater {

namespace {

const base::FilePath::CharType kKeystonePlist[] =
    FILE_PATH_LITERAL("Google/GoogleSoftwareUpdate/GoogleSoftwareUpdate.bundle/"
                      "Contents/Info.plist");

// Gets a value from the updater settings.
template <class T>
T* GetUpdaterSettingsValue(NSString* value_name) {
  id plist_type = CFBridgingRelease(
      CFPreferencesCopyAppValue(base::apple::NSToCFPtrCast(value_name),
                                CFSTR("com.google.Keystone.Agent")));

  return base::apple::ObjCCastStrict<T>(plist_type);
}

base::Time GetUpdaterSettingsTime(NSString* value_name) {
  NSDate* date = GetUpdaterSettingsValue<NSDate>(value_name);
  base::Time result =
      base::Time::FromCFAbsoluteTime([date timeIntervalSinceReferenceDate]);

  return result;
}

base::Version GetVersionFromPlist(const base::FilePath& info_plist) {
  @autoreleasepool {
    NSData* data = [NSData
        dataWithContentsOfFile:base::apple::FilePathToNSString(info_plist)];
    if ([data length] == 0) {
      return base::Version();
    }
    NSDictionary* all_keys =
        base::apple::ObjCCastStrict<NSDictionary>([NSPropertyListSerialization
            propertyListWithData:data
                         options:NSPropertyListImmutable
                          format:nil
                           error:nil]);
    if (all_keys == nil) {
      return base::Version();
    }
    CFStringRef version = base::apple::GetValueFromDictionary<CFStringRef>(
        base::apple::NSToCFPtrCast(all_keys), kCFBundleVersionKey);
    if (version == nullptr) {
      return base::Version();
    }
    return base::Version(base::SysCFStringRefToUTF8(version));
  }
}

}  // namespace

std::string UpdaterState::StateReaderKeystone::GetUpdaterName() const {
  return std::string("Keystone");
}

base::Version UpdaterState::StateReaderKeystone::GetUpdaterVersion(
    bool /*is_machine*/) const {
  // System Keystone takes precedence over user one, so check this one first.
  base::FilePath local_library;
  if (base::apple::GetLocalDirectory(NSLibraryDirectory, &local_library)) {
    base::FilePath system_bundle_plist = local_library.Append(kKeystonePlist);
    base::Version system_keystone = GetVersionFromPlist(system_bundle_plist);
    if (system_keystone.IsValid()) {
      return system_keystone;
    }
  }
  base::FilePath user_bundle_plist =
      base::apple::GetUserLibraryPath().Append(kKeystonePlist);
  return GetVersionFromPlist(user_bundle_plist);
}

base::Time UpdaterState::StateReaderKeystone::GetUpdaterLastStartedAU(
    bool /*is_machine*/) const {
  return GetUpdaterSettingsTime(@"lastCheckStartDate");
}

base::Time UpdaterState::StateReaderKeystone::GetUpdaterLastChecked(
    bool /*is_machine*/) const {
  return GetUpdaterSettingsTime(@"lastServerCheckDate");
}

bool UpdaterState::StateReaderKeystone::IsAutoupdateCheckEnabled() const {
  return UpdaterState::IsAutoupdateCheckEnabled();
}

int UpdaterState::StateReaderKeystone::GetUpdatePolicy() const {
  return UpdaterState::GetUpdatePolicy();
}

update_client::CategorizedError
UpdaterState::StateReaderKeystone::GetLastUpdateCheckError() const {
  return {};
}

bool UpdaterState::IsAutoupdateCheckEnabled() {
  // Auto-update check period override (in seconds).
  // Applies only to older versions of Keystone.
  Boolean foundValue = false;
  long value = CFPreferencesGetAppIntegerValue(
      CFSTR("checkInterval"), CFSTR("com.google.Keystone.Agent"), &foundValue);
  return !foundValue || (0 < value && value < (24 * 60 * 60));
}

int UpdaterState::GetUpdatePolicy() {
  return -1;  // Keystone does not support update policies.
}

}  // namespace component_updater
