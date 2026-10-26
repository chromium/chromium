// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_

#include <string_view>
#include <utility>

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
