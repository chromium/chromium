// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_COORDINATOR_FEATURES_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_COORDINATOR_FEATURES_H_

#include <functional>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"

namespace base {

struct MemoryConsumerTraits;

// When enabled, MemoryPressureListeners/MemoryConsumers that support this
// feature will change their cache's max size for the duration of memory
// pressure, instead of simply evicting entries.
BASE_EXPORT BASE_DECLARE_FEATURE(kStatefulMemoryPressure);

// When enabled, LRUCache instances with a max_size will register as a
// MemoryConsumer with the MemoryCoordinator to adapt their size to memory
// pressure.
BASE_EXPORT BASE_DECLARE_FEATURE(kLRUCacheMemoryConsumer);

// When enabled, allows overriding the mode of individual MemoryConsumers, by
// name. Each param maps a consumer name to its mode, which is one of:
//   - "disabled": The consumer is never forwarded to the memory
//     coordinator, and thus keeps the default memory limit forever.
//   - "stateless": The consumer uses its stateless memory pressure behavior.
//   - "stateful": The consumer uses its stateful memory pressure behavior.
// Consumers that are not listed use the state of `kStatefulMemoryPressure`.
//
// "disabled" is applied by the MemoryConsumerRegistry once the FeatureList is
// initialized, including to the consumers that registered before that.
//
// "stateless" and "stateful" are applied by ShouldUseStatefulMemoryPressure(),
// so they only take effect for consumers that call it instead of checking
// `kStatefulMemoryPressure` directly.
//
// Example:
// --enable-features=MemoryConsumerModeOverrides:PrerenderHostRegistry/disabled/LRURendererCache/stateless
BASE_EXPORT BASE_DECLARE_FEATURE(kMemoryConsumerModeOverrides);

// Returns true if the consumer named `consumer_name` should use its stateful
// memory pressure behavior: its "stateless" or "stateful" override in
// `kMemoryConsumerModeOverrides` if it has one, otherwise the state of
// `kStatefulMemoryPressure`. Must be called after the final FeatureList is
// set, since the parsed overrides are cached for the lifetime of the process.
// In particular, it must not be called while an early-access FeatureList is
// set (see FeatureList::SetEarlyAccessInstance()). Can be called from any
// thread.
BASE_EXPORT bool ShouldUseStatefulMemoryPressure(
    std::string_view consumer_name);

// Returns true if the consumer named `consumer_name` with `traits` operates in
// stateful mode, i.e. it maintains the memory limit it receives. Consumers
// operate in stateless mode regardless of their declared traits when
// ShouldUseStatefulMemoryPressure() returns false for them.
BASE_EXPORT bool IsEffectivelyStateful(std::string_view consumer_name,
                                       const MemoryConsumerTraits& traits);

namespace internal {

// The mode of a MemoryConsumer, as specified via
// `kMemoryConsumerModeOverrides`.
enum class MemoryConsumerMode {
  // The consumer is never forwarded to the memory coordinator.
  kDisabled,
  // The consumer uses its stateless memory pressure behavior.
  kStateless,
  // The consumer uses its stateful memory pressure behavior.
  kStateful,
};

// Maps consumer names to their overridden mode.
using MemoryConsumerModeOverrides =
    flat_map<std::string, MemoryConsumerMode, std::less<>>;

// Parses the overrides specified via `kMemoryConsumerModeOverrides`. Returns
// an empty map if the feature is disabled. Params with an unknown mode are
// ignored. Only intended for use by the MemoryConsumerRegistry and
// ShouldUseStatefulMemoryPressure().
BASE_EXPORT MemoryConsumerModeOverrides ParseMemoryConsumerModeOverrides();

}  // namespace internal

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_COORDINATOR_FEATURES_H_
