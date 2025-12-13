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
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"

namespace base {

class MemoryConsumerRegistration;

// Provides an interface to safely notify MemoryConsumers of their memory limit.
class BASE_EXPORT RegisteredMemoryConsumer {
 public:
  void UpdateMemoryLimit(int percentage);
  void ReleaseMemory();

  friend bool operator==(const RegisteredMemoryConsumer& lhs,
                         const RegisteredMemoryConsumer& rhs) = default;

  int memory_limit() const { return memory_consumer_->memory_limit(); }

 private:
  friend class MemoryConsumerRegistry;

  explicit RegisteredMemoryConsumer(MemoryConsumer* memory_consumer);

  raw_ptr<MemoryConsumer> memory_consumer_;
};

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
  // `consumer_id` and `traits`.
  void AddMemoryConsumer(std::string_view consumer_id,
                         MemoryConsumerTraits traits,
                         MemoryConsumer* consumer);
  void RemoveMemoryConsumer(std::string_view consumer_id,
                            MemoryConsumer* consumer);

  void AddDestructionObserver(
      PassKey<MemoryConsumerRegistration>,
      MemoryConsumerRegistryDestructionObserver* observer);
  void RemoveDestructionObserver(
      PassKey<MemoryConsumerRegistration>,
      MemoryConsumerRegistryDestructionObserver* observer);

 protected:
  RegisteredMemoryConsumer CreateRegisteredMemoryConsumer(
      MemoryConsumer* memory_consumer) {
    return RegisteredMemoryConsumer(memory_consumer);
  }

  // Implementations must call this at the beginning of their destructors.
  // Notifies all registered MemoryConsumerRegistryDestructionObservers.
  void NotifyDestruction();

 private:
  virtual void OnMemoryConsumerAdded(std::string_view consumer_id,
                                     MemoryConsumerTraits traits,
                                     RegisteredMemoryConsumer consumer) = 0;
  virtual void OnMemoryConsumerRemoved(std::string_view consumer_id,
                                       RegisteredMemoryConsumer consumer) = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  ObserverList<MemoryConsumerRegistryDestructionObserver>
      destruction_observers_;
  bool destruction_observers_notified_ = false;
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
