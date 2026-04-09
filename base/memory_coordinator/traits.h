// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_TRAITS_H_
#define BASE_MEMORY_COORDINATOR_TRAITS_H_

#include <stdint.h>

#include <concepts>

#include "base/base_export.h"
#include "base/traits_bag.h"

namespace mojo {
struct DefaultConstruct;
}

namespace base {

namespace internal {

// Helper to make getting an enum from a trait with a default defined in the
// enum more readable.
template <typename Enum, typename... Args>
constexpr Enum GetTraitOrDefault(Args... args) {
  return trait_helpers::GetEnum<Enum, Enum::kDefaultValue>(args...);
}

}  // namespace internal

// Describes how a MemoryConsumer works using a set of enum values.
struct BASE_EXPORT MemoryConsumerTraits {
  // ---- Required Traits ------------------------------------------------------
  // These traits must be set explicitly. There are no default values.

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

  enum class InformationRetention : uint8_t {
    // Freeing memory will result in loss of user state. I.e. discarding a tab.
    kLossy,
    // Freeing memory will not result in the loss of user state. I.e. It is a
    // cache, or it can be recalculated from a raw resource.
    kLossless,

    kMaxValue = kLossless,
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

  // ---- Optional Traits ------------------------------------------------------
  // If not specified, the constructor's template machinery will use the
  // `kDefaultValue` defined in each enum.

  // Indicates if this MemoryConsumer supports the concept of a memory limit.
  enum class SupportsMemoryLimit : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
    kDefaultValue = kYes,
  };

  // Indicates if the memory freed happens inside the process where the consumer
  // lives. If yes, then the consumer can be notified to help in the case of
  // address space exhaustion in the current process.
  enum class InProcess : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
    kDefaultValue = kYes,
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
    kDefaultValue = kNA,
  };

  enum class MemoryReleaseBehavior : uint8_t {
    // OnReleaseMemory() can be called repeatedly to release additional memory.
    // i.e. Tab discarding.
    kRepeatable,
    // Once OnReleaseMemory() is called once, additional calls will not have any
    // effect.  i.e. Cache clearing.
    kIdempotent,

    kMaxValue = kIdempotent,
    kDefaultValue = kIdempotent,
  };

  // Indicates if this consumer manages references to the v8 heap. In this case,
  // no memory is actually released until a garbage collection is done.
  enum class ReleaseGCReferences : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
    kDefaultValue = kNo,
  };

  // Trait for the v8 garbage collector.
  enum class GarbageCollectsV8Heap : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
    kDefaultValue = kNo,
  };

  // Indicates if the consumer is stateful. Stateful consumers (kYes) are
  // preferred as they provide more predictable memory usage. The stateless
  // option (kNo) exists primarily to facilitate the migration of legacy
  // MemoryPressureListener clients.
  //
  // A stateful consumer (kYes) is one that maintains a lasting internal memory
  // limit based on the `memory_limit()` it receives. When memory pressure
  // occurs, it updates this limit and expects to keep it until the next update.
  //
  // A stateless consumer (kNo) is one that does not maintain a lasting limit.
  // Instead, it reacts to memory pressure by performing a one-time eviction of
  // its current entries or resources. Because a stateless consumer doesn't
  // "remember" a restricted state, the memory coordinator will call its
  // `OnReleaseMemory()` method repeatedly if the system remains under pressure,
  // until the pressure is relieved.
  enum class IsStateful : uint8_t {
    kYes,
    kNo,

    kMaxValue = kNo,
    kDefaultValue = kYes,
  };

  // ---- End of traits --------------------------------------------------------

  using RequiredTraitsList = base::ParameterPack<EstimatedMemoryUsage,
                                                 ReleaseMemoryCost,
                                                 InformationRetention,
                                                 ExecutionType>;

  using OptionalTraitsList = base::ParameterPack<SupportsMemoryLimit,
                                                 InProcess,
                                                 RecreateMemoryCost,
                                                 MemoryReleaseBehavior,
                                                 ReleaseGCReferences,
                                                 GarbageCollectsV8Heap,
                                                 IsStateful>;

  using AllTraitsList =
      base::ConcatParameterPacks<RequiredTraitsList, OptionalTraitsList>;

  // Constructs a MemoryConsumerTraits with the specified required traits and
  // zero or more optional traits. Uses the default value for optional traits
  // that are not specified.
  //
  // Examples:
  //   // Only required traits:
  //   constexpr base::MemoryConsumerTraits traits(
  //       base::MemoryConsumerTraits::EstimatedMemoryUsage::kSmall,
  //       base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
  //       base::MemoryConsumerTraits::InformationRetention::kLossy,
  //       base::MemoryConsumerTraits::ExecutionType::kSynchronous);
  //
  //   // Required + some optional traits:
  //   constexpr base::MemoryConsumerTraits traits(
  //       base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
  //       base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
  //       base::MemoryConsumerTraits::InformationRetention::kLossless,
  //       base::MemoryConsumerTraits::ExecutionType::kAsynchronous,
  //       base::MemoryConsumerTraits::InProcess::kNo,
  //       base::MemoryConsumerTraits::IsStateful::kNo);
  //
  template <typename... Args>
    requires trait_helpers::AreValidTraits<OptionalTraitsList, Args...>
  constexpr MemoryConsumerTraits(EstimatedMemoryUsage estimated_memory_usage,
                                 ReleaseMemoryCost release_memory_cost,
                                 InformationRetention information_retention,
                                 ExecutionType execution_type,
                                 Args... args)
      : estimated_memory_usage(estimated_memory_usage),
        release_memory_cost(release_memory_cost),
        information_retention(information_retention),
        execution_type(execution_type),
        supports_memory_limit(
            internal::GetTraitOrDefault<SupportsMemoryLimit>(args...)),
        in_process(internal::GetTraitOrDefault<InProcess>(args...)),
        recreate_memory_cost(
            internal::GetTraitOrDefault<RecreateMemoryCost>(args...)),
        memory_release_behavior(
            internal::GetTraitOrDefault<MemoryReleaseBehavior>(args...)),
        release_gc_references(
            internal::GetTraitOrDefault<ReleaseGCReferences>(args...)),
        garbage_collects_v8_heap(
            internal::GetTraitOrDefault<GarbageCollectsV8Heap>(args...)),
        is_stateful(internal::GetTraitOrDefault<IsStateful>(args...)) {}

  // Mojo-specific constructor used for deserialization.
  //
  // This constructor uses a template parameter to accept the
  // `mojo::DefaultConstruct::Tag` type. This is a workaround for a layering
  // issue where `base` cannot depend on `mojo`, preventing us from including
  // the Mojo header that defines this tag. By using a template and a forward
  // declaration, we can safely reference the tag type without an explicit
  // dependency.
  template <typename T = mojo::DefaultConstruct>
    requires std::same_as<T, mojo::DefaultConstruct>
  constexpr explicit MemoryConsumerTraits(typename T::Tag) {}

  MemoryConsumerTraits(const MemoryConsumerTraits& other);
  MemoryConsumerTraits& operator=(const MemoryConsumerTraits& other);

  friend bool operator==(const MemoryConsumerTraits& lhs,
                         const MemoryConsumerTraits& rhs) = default;

  // LINT.IfChange
  EstimatedMemoryUsage estimated_memory_usage;
  ReleaseMemoryCost release_memory_cost;
  InformationRetention information_retention;
  ExecutionType execution_type;
  SupportsMemoryLimit supports_memory_limit;
  InProcess in_process;
  RecreateMemoryCost recreate_memory_cost;
  MemoryReleaseBehavior memory_release_behavior;
  ReleaseGCReferences release_gc_references;
  GarbageCollectsV8Heap garbage_collects_v8_heap;
  IsStateful is_stateful;
  // LINT.ThenChange(//content/common/memory_coordinator/mojom/memory_coordinator.mojom)
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_TRAITS_H_
