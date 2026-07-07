// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_LIST_INTERNAL_H_
#define BASE_FEATURE_LIST_INTERNAL_H_

#include <functional>
#include <string>

#include "base/base_export.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"

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
  // be subject to change at runtime.
  kRuntimeMutabilityMask = 1 << 18,
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

// State for a runtime-mutable feature. This is stored in the FeatureList's
// map of runtime-mutable features.
struct BASE_EXPORT RuntimeMutableFeatureState {
  RuntimeMutableFeatureState(
      const Feature& feature,
      FeatureList::OnRuntimeMutableFeatureStateChangedCallback callback);
  ~RuntimeMutableFeatureState();

  RuntimeMutableFeatureState(const RuntimeMutableFeatureState&);
  RuntimeMutableFeatureState(RuntimeMutableFeatureState&&);
  RuntimeMutableFeatureState& operator=(const RuntimeMutableFeatureState&);
  RuntimeMutableFeatureState& operator=(RuntimeMutableFeatureState&&);

  // The feature that has runtime mutability enabled.
  std::reference_wrapper<const Feature> feature;

  // Callback to be invoked when the feature state is changed.
  FeatureList::OnRuntimeMutableFeatureStateChangedCallback callback;

  // The runtime override state of the feature, or OVERRIDE_USE_DEFAULT if the
  // feature is not runtime overridden.
  FeatureList::OverrideState override_state = FeatureList::OVERRIDE_USE_DEFAULT;

  // The name and group of the field trial that has, at runtime, superseded
  // the feature's startup-initialized state.
  std::string field_trial_name;
  std::string group_name;
};

}  // namespace base::internal

#endif  // BASE_FEATURE_LIST_INTERNAL_H_
