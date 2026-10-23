// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FEATURE_LIST_INTERNAL_H_
#define BASE_FEATURE_LIST_INTERNAL_H_

#include <stdint.h>

#include <functional>
#include <limits>
#include <optional>
#include <string>

#include "base/base_export.h"
#include "base/feature.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"

namespace base::internal {

// Allocates a caching context, skipping zero when the counter wraps around.
inline uint16_t NextCachingContext(uint16_t& next_context) {
  const uint16_t context = next_context;
  next_context =
      context == std::numeric_limits<uint16_t>::max() ? 1 : context + 1;
  return context;
}

// Masks for the fields in the 32-bit `cached_value`.
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

  // The resolved enabled state and explicit override marker are cached
  // separately so the enabled state can be read without decoding an override.
  kCachedResolvedEnabledMask = 1 << 24,
  kCachedExplicitOverrideMask = 1 << 25,
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
  // kFailure_StateNotSupported = 3,  // Obsolete.
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
      FeatureList::OnRuntimeMutableFeatureStateChangedCallback
          pre_mutation_callback,
      FeatureList::OnRuntimeMutableFeatureStateChangedCallback
          post_mutation_callback);
  ~RuntimeMutableFeatureState();

  RuntimeMutableFeatureState(const RuntimeMutableFeatureState&);
  RuntimeMutableFeatureState(RuntimeMutableFeatureState&&);
  RuntimeMutableFeatureState& operator=(const RuntimeMutableFeatureState&);
  RuntimeMutableFeatureState& operator=(RuntimeMutableFeatureState&&);

  // The feature that has runtime mutability enabled.
  std::reference_wrapper<const Feature> feature;

  // Callback to be invoked right before a state change is about to happen.
  FeatureList::OnRuntimeMutableFeatureStateChangedCallback
      pre_mutation_callback;

  // Callback to be invoked right after a state change happens.
  FeatureList::OnRuntimeMutableFeatureStateChangedCallback
      post_mutation_callback;

  // The runtime override state of the feature, or `std::nullopt` if the
  // feature is not runtime overridden.
  std::optional<FeatureList::OverrideState> override_state;

  // The runtime override info associated with this feature, or null if there is
  // none.
  base::raw_ptr<RuntimeFieldTrialInfo> override_info;
};

}  // namespace base::internal

#endif  // BASE_FEATURE_LIST_INTERNAL_H_
