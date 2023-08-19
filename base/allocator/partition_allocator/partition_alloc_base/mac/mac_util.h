// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MAC_MAC_UTIL_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MAC_MAC_UTIL_H_

#include <AvailabilityMacros.h>
#import <CoreGraphics/CoreGraphics.h>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"

namespace partition_alloc::internal::base::mac {

namespace internal {

// Returns the system's macOS major and minor version numbers combined into an
// integer value. For example, for macOS Sierra this returns 1012, and for macOS
// Big Sur it returns 1100. Note that the accuracy returned by this function is
// as granular as the major version number of Darwin.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) int MacOSVersion();

}  // namespace internal

// Run-time OS version checks. Prefer @available in Objective-C files. If that
// is not possible, use these functions instead of
// base::SysInfo::OperatingSystemVersionNumbers. Prefer the "AtLeast" and
// "AtMost" variants to those that check for a specific version, unless you know
// for sure that you need to check for a specific version.

#define PA_DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsOS10_##V() {                                                 \
    DEPLOYMENT_TARGET_TEST(>, V, false)                                      \
    return internal::MacOSVersion() == 1000 + V;                             \
  }

#define PA_DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsOS##V() {                                                \
    DEPLOYMENT_TARGET_TEST(>, V, false)                                  \
    return internal::MacOSVersion() == V * 100;                          \
  }

#define PA_DEFINE_IS_OS_FUNCS(V, DEPLOYMENT_TARGET_TEST)           \
  PA_DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED(V, DEPLOYMENT_TARGET_TEST) \
  inline bool IsAtLeastOS##V() {                                   \
    DEPLOYMENT_TARGET_TEST(>=, V, true)                            \
    return internal::MacOSVersion() >= V * 100;                    \
  }                                                                \
  inline bool IsAtMostOS##V() {                                    \
    DEPLOYMENT_TARGET_TEST(>, V, false)                            \
    return internal::MacOSVersion() <= V * 100;                    \
  }

#define PA_OLD_TEST_DEPLOYMENT_TARGET(OP, V, RET)               \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_X_VERSION_10_##V) \
    return RET;
#define PA_TEST_DEPLOYMENT_TARGET(OP, V, RET)                  \
  if (MAC_OS_X_VERSION_MIN_REQUIRED OP MAC_OS_VERSION_##V##_0) \
    return RET;
#define PA_IGNORE_DEPLOYMENT_TARGET(OP, V, RET)

// Notes:
// - When bumping the minimum version of the macOS required by Chromium, remove
//   lines from below corresponding to versions of the macOS no longer
//   supported. Ensure that the minimum supported version uses the
//   PA_DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED macro. When macOS 11.0 is the
//   minimum required version, remove all the OLD versions of the macros.
// - When bumping the minimum version of the macOS SDK required to build
//   Chromium, remove the #ifdef that switches between
//   PA_TEST_DEPLOYMENT_TARGET and PA_IGNORE_DEPLOYMENT_TARGET.

// Versions of macOS supported at runtime but whose SDK is not supported for
// building.
PA_DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED(15, PA_OLD_TEST_DEPLOYMENT_TARGET)
PA_DEFINE_IS_OS_FUNCS(11, PA_TEST_DEPLOYMENT_TARGET)
PA_DEFINE_IS_OS_FUNCS(12, PA_TEST_DEPLOYMENT_TARGET)

// Versions of macOS supported at runtime and whose SDK is supported for
// building.
#ifdef MAC_OS_VERSION_13_0
PA_DEFINE_IS_OS_FUNCS(13, PA_TEST_DEPLOYMENT_TARGET)
#else
PA_DEFINE_IS_OS_FUNCS(13, PA_IGNORE_DEPLOYMENT_TARGET)
#endif

#ifdef MAC_OS_VERSION_14_0
PA_DEFINE_IS_OS_FUNCS(14, PA_TEST_DEPLOYMENT_TARGET)
#else
PA_DEFINE_IS_OS_FUNCS(14, PA_IGNORE_DEPLOYMENT_TARGET)
#endif

#undef PA_DEFINE_OLD_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef PA_DEFINE_OLD_IS_OS_FUNCS
#undef PA_DEFINE_IS_OS_FUNCS_CR_MIN_REQUIRED
#undef PA_DEFINE_IS_OS_FUNCS
#undef PA_OLD_TEST_DEPLOYMENT_TARGET
#undef PA_TEST_DEPLOYMENT_TARGET
#undef PA_IGNORE_DEPLOYMENT_TARGET

}  // namespace partition_alloc::internal::base::mac

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_MAC_MAC_UTIL_H_
