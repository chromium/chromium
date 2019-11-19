// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_MAC_UTIL_H_
#define BASE_MAC_MAC_UTIL_H_

#include <stdint.h>
#include <string>

#import <CoreGraphics/CoreGraphics.h>

#include "base/base_export.h"

namespace base {

class FilePath;

namespace mac {

// Full screen modes, in increasing order of priority.  More permissive modes
// take precedence.
enum FullScreenMode {
  kFullScreenModeHideAll = 0,
  kFullScreenModeHideDock = 1,
  kFullScreenModeAutoHideAll = 2,
  kNumFullScreenModes = 3,

  // kFullScreenModeNormal is not a valid FullScreenMode, but it is useful to
  // other classes, so we include it here.
  kFullScreenModeNormal = 10,
};

// Returns an sRGB color space.  The return value is a static value; do not
// release it!
BASE_EXPORT CGColorSpaceRef GetSRGBColorSpace();

// Returns the generic RGB color space. The return value is a static value; do
// not release it!
BASE_EXPORT CGColorSpaceRef GetGenericRGBColorSpace();

// Returns the color space being used by the main display.  The return value
// is a static value; do not release it!
BASE_EXPORT CGColorSpaceRef GetSystemColorSpace();

// Add a full screen request for the given |mode|.  Must be paired with a
// ReleaseFullScreen() call for the same |mode|.  This does not by itself create
// a fullscreen window; rather, it manages per-application state related to
// hiding the dock and menubar.  Must be called on the main thread.
BASE_EXPORT void RequestFullScreen(FullScreenMode mode);

// Release a request for full screen mode.  Must be matched with a
// RequestFullScreen() call for the same |mode|.  As with RequestFullScreen(),
// this does not affect windows directly, but rather manages per-application
// state.  For example, if there are no other outstanding
// |kFullScreenModeAutoHideAll| requests, this will reshow the menu bar.  Must
// be called on main thread.
BASE_EXPORT void ReleaseFullScreen(FullScreenMode mode);

// Convenience method to switch the current fullscreen mode.  This has the same
// net effect as a ReleaseFullScreen(from_mode) call followed immediately by a
// RequestFullScreen(to_mode).  Must be called on the main thread.
BASE_EXPORT void SwitchFullScreenModes(FullScreenMode from_mode,
                                       FullScreenMode to_mode);

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

// Removes the current application from the list Of Login Items.
BASE_EXPORT void RemoveFromLoginItems();

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

// Returns the system's Mac OS X minor version. This is the |y| value
// in 10.y or 10.y.z.
BASE_EXPORT int MacOSXMinorVersion();

}  // namespace internal

// Run-time OS version checks. Prefer @available in Objective-C files. If that
// is not possible, use these functions instead of
// base::SysInfo::OperatingSystemVersionNumbers. Prefer the "AtLeast" and
// "AtMost" variants to those that check for a specific version, unless you know
// for sure that you need to check for a specific version.

#define DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, TEST_DEPLOYMENT_TARGET) \
  inline bool IsOS10_##V() {                                          \
    TEST_DEPLOYMENT_TARGET(>, V, false)                               \
    return internal::MacOSXMinorVersion() == V;                       \
  }                                                                   \
  inline bool IsAtMostOS10_##V() {                                    \
    TEST_DEPLOYMENT_TARGET(>, V, false)                               \
    return internal::MacOSXMinorVersion() <= V;                       \
  }

#define DEFINE_IS_OS_FUNCS(V, TEST_DEPLOYMENT_TARGET)           \
  DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, TEST_DEPLOYMENT_TARGET) \
  inline bool IsAtLeastOS10_##V() {                             \
    TEST_DEPLOYMENT_TARGET(>=, V, true)                         \
    return internal::MacOSXMinorVersion() >= V;                 \
  }

#define TEST_DEPLOYMENT_TARGET(OP, V, RET)                      \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_X_VERSION_10_##V) \
    return RET;
#define IGNORE_DEPLOYMENT_TARGET(OP, V, RET)

// Notes:
// - When bumping the minimum version of the macOS required by Chromium, remove
//   lines from below corresponding to versions of the macOS no longer
//   supported. Ensure that the minimum supported version uses the
//   DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED macro.
// - When bumping the minimum version of the macOS SDK required to build
//   Chromium, remove the #ifdef that switches between TEST_DEPLOYMENT_TARGET
//   and IGNORE_DEPLOYMENT_TARGET.

DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(10, TEST_DEPLOYMENT_TARGET)
DEFINE_IS_OS_FUNCS(11, TEST_DEPLOYMENT_TARGET)
DEFINE_IS_OS_FUNCS(12, TEST_DEPLOYMENT_TARGET)
DEFINE_IS_OS_FUNCS(13, TEST_DEPLOYMENT_TARGET)

#ifdef MAC_OS_X_VERSION_10_14
DEFINE_IS_OS_FUNCS(14, TEST_DEPLOYMENT_TARGET)
#else
DEFINE_IS_OS_FUNCS(14, IGNORE_DEPLOYMENT_TARGET)
#endif

#ifdef MAC_OS_X_VERSION_10_15
DEFINE_IS_OS_FUNCS(15, TEST_DEPLOYMENT_TARGET)
#else
DEFINE_IS_OS_FUNCS(15, IGNORE_DEPLOYMENT_TARGET)
#endif

#undef IGNORE_DEPLOYMENT_TARGET
#undef TEST_DEPLOYMENT_TARGET
#undef DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef DEFINE_IS_OS_FUNCS

// This should be infrequently used. It only makes sense to use this to avoid
// codepaths that are very likely to break on future (unreleased, untested,
// unborn) OS releases, or to log when the OS is newer than any known version.
inline bool IsOSLaterThan10_15_DontCallThis() {
  return !IsAtMostOS10_15();
}

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

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_MAC_UTIL_H_
