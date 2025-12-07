// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_H_

#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer_registry_destruction_observer.h"
#include "base/memory_coordinator/traits.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"

namespace base {

class MemoryConsumerRegistry;

// The MemoryConsumer is used to coordinate memory usage across all processes.
// By registering with the MemoryConsumerRegistry, instances of this class are
// notified when they should reduce or increase memory usage.
//
// Each consumer has a unique ID, and a set of traits. All registered consumers
// with the same unique ID are assumed to be equivalent and must have the exact
// same traits.
//
// Each consumer is assigned a memory usage limit; a percentage that indicates
// how much memory they are allowed to use. The memory coordinator policy does
// not prescribe what a 100% limit is. It is up to the implementer to determine
// what specific number should correspond to the 100% limit.
//
// For instance, if a developer determines that a cache size of 500 entries
// represents the optimal balance between performance and memory usage for a
// typical user on a standard machine, then a 100% memory limit would correspond
// to this size. A 50% memory limit would thus correspond to 250 entries. This
// example uses a linear interpolation, but this is not mandated.
//
// The percentage cannot be negative, but it is allowed to go over 100%, in
// which case the implementation should scale accordingly.
//
// Note that an implementation of `OnUpdateMemoryLimit()` should *not* release
// any memory. This function is intended to simply update an internal limit, and
// a consumer should wait for a subsequent call to `OnReleaseMemory()` to free
// any memory that exceeds that limit.
//
// Here is an example implementation for a consumer that manages a cache with a
// LRU eviction policy.
//
// class ExampleConsumer : public MemoryConsumer {
//  public:
//   // ...
//
//   // MemoryConsumer:
//   void OnUpdateMemoryLimit() override {
//     // Update the maximum size of the cache, but don't decrease that maximum
//     // size below its current size to avoid freeing memory.
//     int new_max_size =
//         std::max(cache_.size(), kDefaultCacheMaxSize * memory_limit() / 100);
//     cache_.SetMaxSize(new_max_size);
//   }
//   void OnReleaseMemory() override {
//     int new_max_size = kDefaultCacheMaxSize * memory_limit() / 100;
//     cache_.SetMaxSizeAndEvictExtraEntries(new_max_size);
//   }
//
//  private:
//   static constexpr int kDefaultCacheMaxSize = 500;
//   LRUCache cache_;
// };
//
class BASE_EXPORT MemoryConsumer {
 public:
  MemoryConsumer();
  virtual ~MemoryConsumer() = default;

  // The memory limit, expressed as a percentage.
  int memory_limit() const { return memory_limit_; }

  // Same as `memory_limit`, but expressed as a ratio.
  double memory_limit_ratio() const { return memory_limit_ / 100.0; }

 protected:
  // Invoked when memory above the current `memory_limit()` should be freed.
  virtual void OnReleaseMemory() = 0;

  // Invoked when the `memory_limit()` assigned to this instance is updated. See
  // the class comment above for a detailed description of how this limit works.
  virtual void OnUpdateMemoryLimit() = 0;

 private:
  friend class RegisteredMemoryConsumer;
  friend class AsyncMemoryConsumerRegistration;

  // Instructs this consumer to update its internal memory limit. See the class
  // comment above for a detailed description of how this limit works.
  void UpdateMemoryLimit(int percentage);

  // Instructs this consumer to release memory that is above the current
  // `memory_limit()`.
  void ReleaseMemory();

  int memory_limit_ = 100;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Similar to ScopedObservation, registers a MemoryConsumer with the global
// MemoryConsumerRegistry.
//
// If `check_unregister` is kEnabled, this class will assert that the
// registration object is destroyed before the destruction of the global
// registry. It can be useful to disable this assert for globals that are
// sometimes leaked.
//
// If `check_registry_exists` is kEnabled, this class will assert that the
// global MemoryConsumerRegistry exists at the time the registration object is
// created. Useful for MemoryConsumers that are used indirectly in tests where
// there are no MemoryConsumerRegistry.
class BASE_EXPORT MemoryConsumerRegistration
    : public MemoryConsumerRegistryDestructionObserver {
 public:
  enum class CheckUnregister {
    kEnabled,
    kDisabled,
  };
  enum class CheckRegistryExists {
    kEnabled,
    kDisabled,
  };

  MemoryConsumerRegistration(
      std::string_view consumer_id,
      MemoryConsumerTraits traits,
      MemoryConsumer* consumer,
      CheckUnregister check_unregister = CheckUnregister::kEnabled,
      CheckRegistryExists check_registry_exists =
          CheckRegistryExists::kEnabled);

  MemoryConsumerRegistration(const MemoryConsumerRegistration&) = delete;
  MemoryConsumerRegistration& operator=(const MemoryConsumerRegistration&) =
      delete;

  ~MemoryConsumerRegistration() override;

  // MemoryConsumerRegistryDestructionObserver:
  void OnBeforeMemoryConsumerRegistryDestroyed() override;

 private:
  using PassKey = PassKey<MemoryConsumerRegistration>;

  std::string consumer_id_;
  raw_ptr<MemoryConsumer> consumer_;

  // Indicates if failure to unregister in time should cause a CHECK failure, or
  // if it should simply be ignored.
  CheckUnregister check_unregister_;

  raw_ptr<MemoryConsumerRegistry> registry_;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_H_
