// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_TRAITS_H_
#define BASE_MEMORY_COORDINATOR_TRAITS_H_

#include <stdint.h>

namespace base {

// Describes how a MemoryConsumer works using a set of enum values.
struct MemoryConsumerTraits {
  // Indicates if this MemoryConsumer supports the concept of a memory limit.
  enum class SupportsMemoryLimit : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
  };

  // Indicates if the memory freed happens inside the process where the consumer
  // lives. If yes, then the consumer can be notified to help in the case of
  // address space exhaustion in the current process.
  enum class InProcess : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
  };

  // The approximate scale of how much memory the consumer can manage.
  enum class EstimatedMemoryUsage : uint8_t {
    // Under 10 MBs.
    kSmall,
    // Tens of MBs.
    kMedium,
    // Hundreds of MBs up to multiple GBs.
    kLarge,

    kMaxValue = kLarge,
  };

  // Indicates if the memory this consumer manages is cheap to free. Traversing
  // a data structure is usually more expensive than freeing a single chunk of
  // memory and can cause memory to be paged in. Note that the page size is
  // typically between 4k and 16k.
  enum class ReleaseMemoryCost : uint8_t {
    // Most of the savings are from allocations larger than the page size that
    // are freed without being accessed.
    kFreesPagesWithoutTraversal,
    // Most of the savings are from allocations smaller than the page size, or
    // from larger allocations that are accessed prior to be freed.
    kRequiresTraversal,

    kMaxValue = kRequiresTraversal,
  };

  // Indicates if recreating the memory is possible, and if so, if is it
  // expensive to do so.
  enum class RecreateMemoryCost : uint8_t {
    // Freed memory can't be recreated.
    kNA,
    // Recreating the memory is not expensive. i.e. Read a file into memory.
    kCheap,
    // Recreating the memory is expensive. i.e. Complex decoding of a resource.
    kExpensive,

    kMaxValue = kExpensive,
  };

  enum class InformationRetention : uint8_t {
    // Freeing memory will result in loss of user state. I.e. discarding a tab.
    kLossy,
    // Freeing memory will not result in the loss of user state. I.e. It is a
    // cache, or it can be recalculated from a raw resource.
    kLossless,

    kMaxValue = kLossless,
  };

  enum class MemoryReleaseBehavior : uint8_t {
    // OnReleaseMemory() can be called repeatedly to release additional memory.
    // i.e. Tab discarding.
    kRepeatable,
    // Once OnReleaseMemory() is called once, additional calls will not have any
    // effect.  i.e. Cache clearing.
    kIdempotent,

    kMaxValue = kIdempotent,
  };

  // Indicates if freeing memory is an asynchronous operation or a synchronous
  // operation. Knowing that a consumer will execute synchronously is useful to
  // know because the memory coordinator policy can then immediately assess the
  // new state of the machine after the notification.
  enum class ExecutionType : uint8_t {
    kSynchronous,
    kAsynchronous,

    kMaxValue = kAsynchronous,
  };

  // Indicates if this consumer manages references to the v8 heap. In this case,
  // no memory is actually released until a garbage collection is done.
  enum class ReleaseGCReferences : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
  };

  // Trait for the v8 garbage collector.
  enum class GarbageCollectsV8Heap : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
  };

  friend bool operator==(const MemoryConsumerTraits& lhs,
                         const MemoryConsumerTraits& rhs) = default;

  // LINT.IfChange
  SupportsMemoryLimit supports_memory_limit;
  InProcess in_process;
  EstimatedMemoryUsage estimated_memory_usage;
  ReleaseMemoryCost release_memory_cost;
  RecreateMemoryCost recreate_memory_cost;
  InformationRetention information_retention;
  MemoryReleaseBehavior memory_release_behavior;
  ExecutionType execution_type;
  ReleaseGCReferences release_gc_references;
  GarbageCollectsV8Heap garbage_collects_v8_heap;
  // LINT.ThenChange(//content/common/memory_coordinator/mojom/memory_coordinator.mojom)
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_TRAITS_H_
