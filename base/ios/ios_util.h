// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IOS_IOS_UTIL_H_
#define BASE_IOS_IOS_UTIL_H_

#include <stdint.h>

#include "base/base_export.h"
#include "base/files/file_path.h"

namespace base {
namespace ios {

// Returns whether the operating system is iOS 10 or later.
BASE_EXPORT bool IsRunningOnIOS10OrLater();

// Returns whether the operating system is iOS 11 or later.
BASE_EXPORT bool IsRunningOnIOS11OrLater();

// Returns whether the operating system is iOS 12 or later.
BASE_EXPORT bool IsRunningOnIOS12OrLater();

// Returns whether the operating system is iOS 13 or later.
BASE_EXPORT bool IsRunningOnIOS13OrLater();

// Returns whether the operating system is at the given version or later.
BASE_EXPORT bool IsRunningOnOrLater(int32_t major,
                                    int32_t minor,
                                    int32_t bug_fix);

// Returns whether iOS is signalling that an RTL text direction should be used
// regardless of the current locale. This should not return true if the current
// language is a "real" RTL language such as Arabic or Urdu; it should only
// return true in cases where the RTL text direction has been forced (for
// example by using the "RTL Psuedolanguage" option when launching from XCode).
BASE_EXPORT bool IsInForcedRTL();

// Stores the |path| of the ICU dat file in a global to be referenced later by
// FilePathOfICUFile().  This should only be called once.
BASE_EXPORT void OverridePathOfEmbeddedICU(const char* path);

// Returns the overriden path set by OverridePathOfEmbeddedICU(), otherwise
// returns invalid FilePath.
BASE_EXPORT FilePath FilePathOfEmbeddedICU();

}  // namespace ios
}  // namespace base

#endif  // BASE_IOS_IOS_UTIL_H_
