// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer_registry_destruction_observer.h"
#include "base/memory_coordinator/traits.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"

namespace base {

class AsyncMemoryConsumerRegistration;
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
// IMPORTANT: For synchronous registrations (via `MemoryConsumerRegistration`),
// `OnUpdateMemoryLimit()` is NOT invoked during registration to avoid
// re-entrancy during construction. If your implementation maintains state
// derived from the limit, you must query `memory_limit()` in your constructor
// body to initialize it correctly. (Asynchronous registrations do not have
// this limitation as they notify asynchronously after construction).
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
//     int target_cache_size = ScaleByMemoryLimit(kDefaultCacheMaxSize,
//                                                memory_limit());
//     cache_.SetMaxSize(std::max(cache_.size(), target_cache_size));
//   }
//   void OnReleaseMemory() override {
//     cache_.SetMaxSizeAndEvictExtraEntries(
//         ScaleByMemoryLimit(kDefaultCacheMaxSize, memory_limit()));
//   }
//
//  private:
//   static constexpr int kDefaultCacheMaxSize = 500;
//   LRUCache cache_;
// };
//
// Note: If you need to support multiple memory interventions in the same class,
// do not inherit from MemoryConsumer directly. Instead, use
// MultiMemoryConsumer (defined in
// base/memory_coordinator/multi_memory_consumer.h) which is specifically
// designed to support multiple registrations.
//
class BASE_EXPORT MemoryConsumer {
 public:
  // This is the default value for a consumer's memory limit. It corresponds to
  // 100%, meaning the consumer is not restricted in its memory usage.
  static constexpr int kDefaultMemoryLimit = 100;
  static constexpr double kDefaultMemoryLimitRatio = 1.0;

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

  // Similar to UpdateMemoryLimit, but does not invoke OnUpdateMemoryLimit
  // callback.
  void UpdateMemoryLimitNoNotification(int percentage);

  // Instructs this consumer to release memory that is above the current
  // `memory_limit()`.
  void ReleaseMemory();

  int memory_limit_ = kDefaultMemoryLimit;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A PassiveMemoryConsumer is a MemoryConsumer that does not react to memory
// pressure. It is intended for consumers that only need to query the current
// memory limit.
class BASE_EXPORT PassiveMemoryConsumer : public MemoryConsumer {
 public:
  // MemoryConsumer:
  void OnReleaseMemory() final {}
  void OnUpdateMemoryLimit() final {}
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

  // `traits` is only optional temporarily to assist with the migration of
  // clients from MemoryPressureListener to MemoryCoordinator. It will be made
  // mandatory in the future.
  MemoryConsumerRegistration(
      std::string_view consumer_name,
      std::optional<MemoryConsumerTraits> traits,
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

  // Associates an optional flag that indicates if the async handle of this
  // registration was already destroyed.
  void SetAsyncHandleDestroyedFlag(
      const std::atomic<bool>* async_handle_destroyed_flag,
      base::PassKey<AsyncMemoryConsumerRegistration> pass_key);

 private:
  using PassKey = base::PassKey<MemoryConsumerRegistration>;

  std::string consumer_name_;
  raw_ptr<MemoryConsumer> consumer_;

  // Whether we should check if the consumer was correctly unregistered or
  // if it should simply be ignored.
  CheckUnregister check_unregister_;

  // An optional flag that indicates if the async handle of this registration
  // was already destroyed. This is used to distinguish between a leak and a
  // race condition in async registrations.
  raw_ptr<const std::atomic<bool>> async_handle_destroyed_flag_;

  raw_ptr<MemoryConsumerRegistry> registry_;
};

// Scales a baseline value linearly by the provided `memory_limit` (expressed as
// a percentage, e.g., 100 for 1.0x).
//
// The result is truncated towards zero and clamped to the range of the
// type to prevent overflow.
template <typename T>
T ScaleByMemoryLimit(T baseline, int memory_limit) {
  static_assert(std::is_integral_v<T>, "T must be an integral type.");
  CHECK_GE(memory_limit, 0);
  // Calculate the ratio first (memory_limit / 100.0) to avoid potential
  // overflow during multiplication and then scale the baseline.
  double ratio = memory_limit / 100.0;
  return base::saturated_cast<T>(baseline * ratio);
}

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_H_
