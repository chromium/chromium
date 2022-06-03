// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MAC_UTIL_H_
#define BASE_MAC_MAC_UTIL_H_

#include <AvailabilityMacros.h>
#import <CoreGraphics/CoreGraphics.h>
#include <stdint.h>

#include <string>

#include "base/base_export.h"

namespace base {

class FilePath;

namespace mac {

// Returns an sRGB color space.  The return value is a static value; do not
// release it!
BASE_EXPORT CGColorSpaceRef GetSRGBColorSpace();

// Returns the generic RGB color space. The return value is a static value; do
// not release it!
BASE_EXPORT CGColorSpaceRef GetGenericRGBColorSpace();

// Returns the color space being used by the main display.  The return value
// is a static value; do not release it!
BASE_EXPORT CGColorSpaceRef GetSystemColorSpace();

// Returns true if the file at |file_path| is excluded from Time Machine
// backups.
BASE_EXPORT bool GetFileBackupExclusion(const FilePath& file_path);

// Excludes the file given by |file_path| from Time Machine backups.
BASE_EXPORT bool SetFileBackupExclusion(const FilePath& file_path);

// Checks if the current application is set as a Login Item, so it will launch
// on Login. If a non-NULL pointer to is_hidden is passed, the Login Item also
// is queried for the 'hide on launch' flag.
BASE_EXPORT bool CheckLoginItemStatus(bool* is_hidden);

// Adds current application to the set of Login Items with specified "hide"
// flag. This has the same effect as adding/removing the application in
// SystemPreferences->Accounts->LoginItems or marking Application in the Dock
// as "Options->Open on Login".
// Does nothing if the application is already set up as Login Item with
// specified hide flag.
BASE_EXPORT void AddToLoginItems(bool hide_on_startup);

// Adds the specified application to the set of Login Items with specified
// "hide" flag. This has the same effect as adding/removing the application in
// SystemPreferences->Accounts->LoginItems or marking Application in the Dock
// as "Options->Open on Login".
// Does nothing if the application is already set up as Login Item with
// specified hide flag.
BASE_EXPORT void AddToLoginItems(const FilePath& app_bundle_file_path,
                                 bool hide_on_startup);

// Removes the current application from the list Of Login Items.
BASE_EXPORT void RemoveFromLoginItems();

// Removes the specified application from the list Of Login Items.
BASE_EXPORT void RemoveFromLoginItems(const FilePath& app_bundle_file_path);

// Returns true if the current process was automatically launched as a
// 'Login Item' or via Lion's Resume. Used to suppress opening windows.
BASE_EXPORT bool WasLaunchedAsLoginOrResumeItem();

// Returns true if the current process was automatically launched as a
// 'Login Item' or via Resume, and the 'Reopen windows when logging back in'
// checkbox was selected by the user.  This indicates that the previous
// session should be restored.
BASE_EXPORT bool WasLaunchedAsLoginItemRestoreState();

// Returns true if the current process was automatically launched as a
// 'Login Item' with 'hide on startup' flag. Used to suppress opening windows.
BASE_EXPORT bool WasLaunchedAsHiddenLoginItem();

// Remove the quarantine xattr from the given file. Returns false if there was
// an error, or true otherwise.
BASE_EXPORT bool RemoveQuarantineAttribute(const FilePath& file_path);

namespace internal {

// Returns the system's macOS major and minor version numbers combined into an
// integer value. For example, for macOS Sierra this returns 1012, and for macOS
// Big Sur it returns 1100. Note that the accuracy returned by this function is
// as granular as the major version number of Darwin.
BASE_EXPORT int MacOSVersion();

}  // namespace internal

// Run-time OS version checks. Prefer @available in Objective-C files. If that
// is not possible, use these functions instead of
// base::SysInfo::OperatingSystemVersionNumbers. Prefer the "AtLeast" and
// "AtMost" variants to those that check for a specific version, unless you know
// for sure that you need to check for a specific version.

#define DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsOS10_##V() {                                              \
    DEPLOYMENT_TARGET_TEST(>, V, false)                                   \
    return internal::MacOSVersion() == 1000 + V;                          \
  }                                                                       \
  inline bool IsAtMostOS10_##V() {                                        \
    DEPLOYMENT_TARGET_TEST(>, V, false)                                   \
    return internal::MacOSVersion() <= 1000 + V;                          \
  }

#define DEFINE_OLD_IS_OS_FUNCS(V, DEPLOYMENT_TARGET_TEST)           \
  DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsAtLeastOS10_##V() {                                 \
    DEPLOYMENT_TARGET_TEST(>=, V, true)                             \
    return internal::MacOSVersion() >= 1000 + V;                    \
  }

#define DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsOS##V() {                                             \
    DEPLOYMENT_TARGET_TEST(>, V, false)                               \
    return internal::MacOSVersion() == V * 100;                       \
  }                                                                   \
  inline bool IsAtMostOS##V() {                                       \
    DEPLOYMENT_TARGET_TEST(>, V, false)                               \
    return internal::MacOSVersion() <= V * 100;                       \
  }

#define DEFINE_IS_OS_FUNCS(V, DEPLOYMENT_TARGET_TEST)           \
  DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsAtLeastOS##V() {                                \
    DEPLOYMENT_TARGET_TEST(>=, V, true)                         \
    return internal::MacOSVersion() >= V * 100;                 \
  }

#define OLD_TEST_DEPLOYMENT_TARGET(OP, V, RET)                  \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_X_VERSION_10_##V) \
    return RET;
#define TEST_DEPLOYMENT_TARGET(OP, V, RET)                     \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_VERSION_##V##_0) \
    return RET;
#define IGNORE_DEPLOYMENT_TARGET(OP, V, RET)

// Notes:
// - When bumping the minimum version of the macOS required by Chromium, remove
//   lines from below corresponding to versions of the macOS no longer
//   supported. Ensure that the minimum supported version uses the
//   DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED macro. When macOS 11.0 is the
//   minimum required version, remove all the OLD versions of the macros.
// - When bumping the minimum version of the macOS SDK required to build
//   Chromium, remove the #ifdef that switches between
//   TEST_DEPLOYMENT_TARGET and IGNORE_DEPLOYMENT_TARGET.

// Versions of macOS supported at runtime but whose SDK is not supported for
// building.
DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(11, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(12, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(13, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(14, OLD_TEST_DEPLOYMENT_TARGET)
DEFINE_OLD_IS_OS_FUNCS(15, OLD_TEST_DEPLOYMENT_TARGET)

// Versions of macOS supported at runtime and whose SDK is supported for
// building.
#ifdef MAC_OS_VERSION_11_0
DEFINE_IS_OS_FUNCS(11, TEST_DEPLOYMENT_TARGET)
#else
DEFINE_IS_OS_FUNCS(11, IGNORE_DEPLOYMENT_TARGET)
#endif

#ifdef MAC_OS_VERSION_12_0
DEFINE_IS_OS_FUNCS(12, TEST_DEPLOYMENT_TARGET)
#else
DEFINE_IS_OS_FUNCS(12, IGNORE_DEPLOYMENT_TARGET)
#endif

#undef DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef DEFINE_OLD_IS_OS_FUNCS
#undef DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef DEFINE_IS_OS_FUNCS
#undef OLD_TEST_DEPLOYMENT_TARGET
#undef TEST_DEPLOYMENT_TARGET
#undef IGNORE_DEPLOYMENT_TARGET

// This should be infrequently used. It only makes sense to use this to avoid
// codepaths that are very likely to break on future (unreleased, untested,
// unborn) OS releases, or to log when the OS is newer than any known version.
inline bool IsOSLaterThan12_DontCallThis() {
  return !IsAtMostOS12();
}

enum class CPUType {
  kIntel,
  kTranslatedIntel,  // Rosetta
  kArm,
};

// Returns the type of CPU this is being executed on.
BASE_EXPORT CPUType GetCPUType();

// Retrieve the system's model identifier string from the IOKit registry:
// for example, "MacPro4,1", "MacBookPro6,1". Returns empty string upon
// failure.
BASE_EXPORT std::string GetModelIdentifier();

// Parse a model identifier string; for example, into ("MacBookPro", 6, 1).
// If any error occurs, none of the input pointers are touched.
BASE_EXPORT bool ParseModelIdentifier(const std::string& ident,
                                      std::string* type,
                                      int32_t* major,
                                      int32_t* minor);

// Returns an OS name + version string. e.g.:
//
//   "macOS Version 10.14.3 (Build 18D109)"
//
// Parts of this string change based on OS locale, so it's only useful for
// displaying to the user.
BASE_EXPORT std::string GetOSDisplayName();

// Returns the serial number of the macOS device.
BASE_EXPORT std::string GetPlatformSerialNumber();

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_MAC_UTIL_H_
