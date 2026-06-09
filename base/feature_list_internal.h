// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_LIST_INTERNAL_H_
#define BASE_FEATURE_LIST_INTERNAL_H_

#include "base/feature.h"

namespace base::internal {

// Each of these masks corresponds to one or more bits between bits 16 and 23
// of the 32-bit `cached_value` and is used to check or set particular flags
// tracking the use or usability of the feature.
enum : Feature::FeatureStateCache {
  // The bottom 16 bits of the 32-bit cached value are used for the caching
  // context ID.
  kCachingContextMask = 0xFFFF,

  // The 8 bits from 16 to 23 of the 32-bit cached value are used for flags.
  kAllFlagsMask = 0xFF << 16,

  // The feature has been accessed, in general.
  kCachedLogGeneralMask = 1 << 16,

  // The feature has been accessed before the FeatureList was initialized.
  kCachedLogEarlyMask = 1 << 17,

  // The feature has been declared as runtime mutable. The feature's state may
  // be subject to change at runtime, depending on the state of
  // `kRuntimeMutabilityEnabledMask` and `kRuntimeMutabilityDisabledMask`.
  kRuntimeMutabilityMask = 1 << 18,

  // Set when EnableRuntimeMutability() is successfully called on a runtime
  // mutable feature, this flag indicates that the feature is known to the
  // objects that manage runtime mutability and is properly configured. The
  // feature's state may change at runtime.
  kRuntimeMutabilityEnabledMask = 1 << 19,

  // The feature has had its runtime mutability disabled. This happens if a
  // runtime mutable feature is accessed before the feature's runtime mutability
  // has been enabled. Note that having runtime mutability disabled is not the
  // same as not having runtime mutability enabled. Having runtime mutability
  // disabled means that the feature is in a bad state wherein its runtime
  // mutability has been rendered non-functional.
  //
  // This state may be detected and set via two scenarios:
  // 1. The runtime mutable feature was accessed before the FeatureList was
  //    initialized. The state can be detected if/when EnableRuntimeMutability()
  //    is eventually called for the already-accessed feature.
  // 2. EnableRuntimeMutability() was never called for the feature. This state
  //    can be detected on the first access of the feature.
  kRuntimeMutabilityDisabledMask = 1 << 20,
};

// Result of a runtime-mutable feature operation. These values are logged to
// UMA, so should not be reordered or have values reused.
enum class RuntimeMutabilityResult {
  // Reserve the default/uninitialized value. This should not be used.
  kUnknown = 0,
  // The runtime-update was successful.
  kSuccess = 1,
  // A runtime-update was rejected because the targeted feature was not found
  // to be enabled for runtime mutability.
  kFailure = 2,
  // A runtime-update was rejected because the requested feature state is not
  // supported. V0, for example, only supports the disabling of runtime-mutable
  // features.
  kFailure_StateNotSupported = 3,
  // The runtime-mutability of a feature has been disabled because the feature's
  // state was set via a command-line override. This is not an error, per se,
  // but is still worth logging to understand whether runtime mutability is
  // being affected by command-line overrides.
  kFailure_CommandLineOverride = 4,
  // Add new values above this line, and update kMaxValue below.
  kMaxValue = kFailure_CommandLineOverride,
};

}  // namespace base::internal

#endif  // BASE_FEATURE_LIST_INTERNAL_H_
