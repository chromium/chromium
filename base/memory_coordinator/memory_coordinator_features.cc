// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_coordinator_features.h"

#include <optional>

#include "base/memory_coordinator/traits.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "build/build_config.h"

namespace base {

BASE_FEATURE(kStatefulMemoryPressure,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
BASE_FEATURE(kLRUCacheMemoryConsumer, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kMemoryConsumerModeOverrides, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

std::optional<internal::MemoryConsumerMode> ParseMemoryConsumerMode(
    std::string_view mode) {
  if (mode == "disabled") {
    return internal::MemoryConsumerMode::kDisabled;
  }
  if (mode == "stateless") {
    return internal::MemoryConsumerMode::kStateless;
  }
  if (mode == "stateful") {
    return internal::MemoryConsumerMode::kStateful;
  }
  return std::nullopt;
}

// Returns the overridden mode of the consumer named `consumer_name`, if any.
std::optional<internal::MemoryConsumerMode> GetMemoryConsumerModeOverride(
    std::string_view consumer_name) {
  // Avoid parsing the overrides in the common case.
  if (!FeatureList::IsEnabled(kMemoryConsumerModeOverrides)) {
    return std::nullopt;
  }

  auto find_mode =
      [consumer_name](const internal::MemoryConsumerModeOverrides& overrides)
      -> std::optional<internal::MemoryConsumerMode> {
    auto it = overrides.find(consumer_name);
    if (it == overrides.end()) {
      return std::nullopt;
    }
    return it->second;
  };

  // Like BASE_FEATURE_PARAM, cache the parsed overrides unless the param cache
  // is disabled, which ScopedFeatureList does when it overrides params.
  if (!internal::IsFeatureParamWithCacheEnabled()) {
    return find_mode(internal::ParseMemoryConsumerModeOverrides());
  }
  static const NoDestructor<internal::MemoryConsumerModeOverrides>
      cached_overrides(internal::ParseMemoryConsumerModeOverrides());
  return find_mode(*cached_overrides);
}

}  // namespace

bool ShouldUseStatefulMemoryPressure(std::string_view consumer_name) {
  std::optional<internal::MemoryConsumerMode> mode =
      GetMemoryConsumerModeOverride(consumer_name);
  if (mode == internal::MemoryConsumerMode::kStateless) {
    return false;
  }
  if (mode == internal::MemoryConsumerMode::kStateful) {
    return true;
  }
  // Consumers without a stateless or stateful override, including disabled
  // ones, follow the global feature.
  return FeatureList::IsEnabled(kStatefulMemoryPressure);
}

bool IsEffectivelyStateful(std::string_view consumer_name,
                           const MemoryConsumerTraits& traits) {
  return ShouldUseStatefulMemoryPressure(consumer_name) &&
         traits.is_stateful == MemoryConsumerTraits::IsStateful::kYes;
}

namespace internal {

MemoryConsumerModeOverrides ParseMemoryConsumerModeOverrides() {
  MemoryConsumerModeOverrides overrides;
  if (!FeatureList::IsEnabled(kMemoryConsumerModeOverrides)) {
    return overrides;
  }

  // Each param maps a consumer name to its mode.
  FieldTrialParams params;
  if (!GetFieldTrialParamsByFeature(kMemoryConsumerModeOverrides, &params)) {
    return overrides;
  }
  for (const auto& [consumer_name, mode_string] : params) {
    if (std::optional<MemoryConsumerMode> mode =
            ParseMemoryConsumerMode(mode_string)) {
      overrides.emplace(consumer_name, *mode);
    }
  }
  return overrides;
}

}  // namespace internal

}  // namespace base
