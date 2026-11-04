// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_

#include <stdint.h>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry_destruction_observer.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace base {

class MemoryConsumerRegistration;

// A base class for registering a MemoryConsumer with the global registry for
// the current process.
class BASE_EXPORT MemoryConsumerRegistry {
 public:
  static bool Exists();
  static MemoryConsumerRegistry& Get();
  static MemoryConsumerRegistry* MaybeGet();
  static void Set(MemoryConsumerRegistry* instance);

  MemoryConsumerRegistry();
  virtual ~MemoryConsumerRegistry();

  // Adds/Removes an instance of MemoryConsumer with a specific
  // `consumer_name` and `traits`.
  void AddMemoryConsumer(std::string_view consumer_name,
                         MemoryConsumerTraits traits,
                         MemoryConsumer* consumer);
  void RemoveMemoryConsumer(std::string_view consumer_name,
                            MemoryConsumer* consumer);

  void AddDestructionObserver(
      PassKey<MemoryConsumerRegistration>,
      MemoryConsumerRegistryDestructionObserver* observer);
  void RemoveDestructionObserver(
      PassKey<MemoryConsumerRegistration>,
      MemoryConsumerRegistryDestructionObserver* observer);

  // Must be called once, after the FeatureList is initialized. Applies the
  // "disabled" overrides specified via `kMemoryConsumerModeOverrides`. This is
  // separate from the constructor because the registry may be created before
  // the FeatureList. Consumers that were registered before this call and are
  // disabled are removed from the implementation. Thus, to guarantee that a
  // disabled consumer never receives any notification, implementations must
  // not notify consumers before this call.
  void OnFeatureListInitialized();

 protected:
  // Helpers to notify consumers of memory events.
  static void NotifyReleaseMemory(MemoryConsumer* consumer);
  static void NotifyUpdateMemoryLimit(MemoryConsumer* consumer,
                                      MemoryLimit memory_limit);

  // Implementations must call this at the beginning of their destructors.
  // Notifies all registered MemoryConsumerRegistryDestructionObservers.
  void NotifyDestruction();

 protected:
  virtual void OnMemoryConsumerAdded(uint32_t consumer_id,
                                     std::string_view consumer_name,
                                     MemoryConsumerTraits traits,
                                     MemoryConsumer* consumer) = 0;
  virtual void OnMemoryConsumerRemoved(uint32_t consumer_id,
                                       MemoryConsumer* consumer) = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // A hash set is used instead of an ObserverList because every
  // MemoryConsumerRegistration in the process is added, making the linear
  // add/remove of ObserverList costly. Erasing never rehashes, so observers can
  // safely remove themselves during NotifyDestruction().
  absl::flat_hash_set<MemoryConsumerRegistryDestructionObserver*>
      destruction_observers_;
  bool destruction_observers_notified_ = false;

  // True while NotifyDestruction() is iterating over `destruction_observers_`.
  // Adding an observer during that time could rehash the set and invalidate
  // the iteration.
  bool is_notifying_destruction_ = false;

 private:
  // The IDs of the consumers disabled via `kMemoryConsumerModeOverrides`.
  // Disabled consumers are never forwarded to the implementation, and thus keep
  // the default memory limit. The other modes are handled by
  // ShouldUseStatefulMemoryPressure().
  //
  // Unset until OnFeatureListInitialized() is called.
  std::optional<absl::flat_hash_set<uint32_t>> disabled_consumer_ids_;

  // The consumers that are registered before OnFeatureListInitialized() is
  // called, with their ID, so that the disabled ones can be removed from the
  // implementation once the overrides are known. Cleared by
  // OnFeatureListInitialized().
  std::vector<std::pair<uint32_t, raw_ptr<MemoryConsumer>>> early_consumers_;
};

// Helper class for creating and registering a singleton registry. This is
// separate from the base interface so that tests can instantiate multiple
// registries in the same process.
template <class RegistryType>
class ScopedMemoryConsumerRegistry {
 public:
  template <class... Args>
  explicit ScopedMemoryConsumerRegistry(Args&&... args)
      : registry_(std::forward<Args>(args)...) {
    MemoryConsumerRegistry::Set(&registry_);
  }

  ScopedMemoryConsumerRegistry(const ScopedMemoryConsumerRegistry&) = delete;
  ScopedMemoryConsumerRegistry operator=(ScopedMemoryConsumerRegistry&) =
      delete;

  ~ScopedMemoryConsumerRegistry() { MemoryConsumerRegistry::Set(nullptr); }

  RegistryType& Get() { return registry_; }

 private:
  RegistryType registry_;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_
