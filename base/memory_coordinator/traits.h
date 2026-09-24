// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_TRAITS_H_
#define BASE_MEMORY_COORDINATOR_TRAITS_H_

#include <stdint.h>

#include <concepts>

#include "base/base_export.h"
#include "base/check_op.h"
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

// Helper to get an enum trait with a custom default value.
template <typename Enum, Enum Default, typename... Args>
constexpr Enum GetTraitOrCustomDefault(Args... args) {
  return trait_helpers::GetEnum<Enum, Default>(args...);
}

}  // namespace internal

// Describes how a MemoryConsumer works using a set of enum values.
//
// For active consumers, these traits describe how they reclaim memory and the
// cost of doing so. For passive consumers, a simplified set of traits is used
// (see the kPassive constructor).
struct BASE_EXPORT MemoryConsumerTraits {
  // Indicates whether the consumer is active or passive.
  //
  // Active consumers (kActive) react to memory pressure by actively releasing
  // memory when notified. They require a full set of traits to describe their
  // behavior. This trait is optional for active consumers and defaults to
  // `kActive`.
  //
  // Passive consumers (kPassive) do not react to memory pressure. They only
  // query the memory limit on-demand to gate their activities. They do not
  // require active traits as they are mostly non-applicable.
  enum class ConsumerType : uint8_t {
    kActive,
    kPassive,

    kMaxValue = kPassive,
    kDefaultValue = kActive,
  };

  // ---- Required Traits (Active Consumers Only) ------------------------------
  // These traits must be set explicitly for active consumers. There are no
  // default values. (Passive consumers use defaults and do not require these).

  // The approximate scale of how much memory we expect to reclaim when
  // OnReleaseMemory() is called under memory pressure.
  //
  // If the MemoryConsumer holds strong V8 memory references, the actual call
  // to OnReleaseMemory() might only drop the references, deferring the
  // physical memory reclamation until the next GC cycle. This estimation
  // should still include the V8 memory that is expected to be reclaimed
  // once GC runs.
  enum class EstimatedMemoryUsage : uint8_t {
    // Under 10 MBs.
    kSmall,
    // Tens of MBs.
    kMedium,
    // Hundreds of MBs up to multiple GBs.
    kLarge,
    // Not applicable (e.g. for passive consumers).
    kNA,

    kMaxValue = kNA,
  };

  // Indicates the CPU and paging cost of executing the OnReleaseMemory()
  // callback. This should be evaluated based on the work done in the callback
  // itself (e.g. traversing a data structure to drop references), independent
  // of whether physical memory reclamation is deferred to garbage collection.
  enum class ReleaseMemoryCost : uint8_t {
    // Most of the savings are from allocations larger than the page size that
    // are freed without being accessed.
    kFreesPagesWithoutTraversal,
    // Most of the savings are from allocations smaller than the page size, or
    // from larger allocations that are accessed prior to be freed.
    kRequiresTraversal,
    // Not applicable (e.g. for passive consumers).
    kNA,

    kMaxValue = kNA,
  };

  enum class InformationRetention : uint8_t {
    // Freeing memory will result in loss of user state. I.e. discarding a tab.
    kLossy,
    // Freeing memory will not result in the loss of user state. I.e. It is a
    // cache, or it can be recalculated from a raw resource.
    kLossless,
    // Not applicable (e.g. for passive consumers).
    kNA,

    kMaxValue = kNA,
  };

  // Indicates if executing the OnReleaseMemory() callback is synchronous
  // or asynchronous. This is independent of whether physical memory
  // reclamation is deferred (e.g., to a garbage collection cycle).
  // Knowing that a callback executes synchronously is useful because the
  // memory coordinator can then immediately take subsequent actions (such as
  // triggering a garbage collection or assessing the new state of the
  // machine) once the callback returns.
  //
  // Note: If `AsyncMemoryConsumerRegistration` is used to register the
  // consumer, the execution should be considered asynchronous.
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
    // Not applicable (e.g. for passive consumers).
    kNA,

    kMaxValue = kNA,
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
  // Because it maintains this limit, it is idempotent: subsequent calls to
  // `OnReleaseMemory()` without a limit update will not have any effect.
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

  // Indicates whether the consumer performs a "recovery", i.e. takes an
  // explicit action to use more memory when its memory limit is raised
  // (typically once memory pressure is over).
  //
  // A consumer whose OnUpdateMemoryLimit() merely raises an internal maximum
  // size does not recover: no memory is used as a result of the update. Such a
  // consumer is kNone, even though normal Chrome usage may later grow its
  // memory usage back up (e.g. a cache refilling as entries are inserted) --
  // that growth is driven by usage, not by the limit change.
  //
  // kNone is the default because it is by far the common case. Consumers that
  // explicitly allocate on a limit increase must set kImmediate.
  enum class RecoveryBehavior : uint8_t {
    // The consumer takes no action to use more memory when its limit is raised.
    kNone,
    // The consumer allocates or repopulates memory as soon as its limit is
    // raised. This has an instant memory cost that risks re-creating the
    // pressure that was just relieved, so the memory coordinator may restore
    // such limits more conservatively.
    kImmediate,
    // Not applicable (e.g. for passive consumers).
    kNA,

    kMaxValue = kNA,
    kDefaultValue = kNone,
  };

  // ---- End of traits --------------------------------------------------------

  using RequiredTraitsList = base::ParameterPack<EstimatedMemoryUsage,
                                                 ReleaseMemoryCost,
                                                 InformationRetention,
                                                 ExecutionType>;

  using OptionalTraitsList = base::ParameterPack<SupportsMemoryLimit,
                                                 InProcess,
                                                 RecreateMemoryCost,
                                                 ReleaseGCReferences,
                                                 GarbageCollectsV8Heap,
                                                 IsStateful,
                                                 RecoveryBehavior,
                                                 ConsumerType>;

  using PassiveOptionalTraitsList =
      base::ParameterPack<SupportsMemoryLimit, InProcess, ReleaseGCReferences>;

  using AllTraitsList =
      base::ConcatParameterPacks<RequiredTraitsList, OptionalTraitsList>;

  // Constructs a MemoryConsumerTraits for a passive memory consumer.
  //
  // Passive consumers only use the memory limit as a binary pressure gate and
  // do not actively release memory. Therefore, all active-only traits (like
  // EstimatedMemoryUsage, ReleaseMemoryCost, etc.) are automatically set to
  // kNA or passive-safe defaults and cannot be customized.
  //
  // A limited subset of optional traits that are relevant for passive consumers
  // (SupportsMemoryLimit, InProcess, and ReleaseGCReferences) can be optionally
  // specified.
  //
  // Examples:
  //   // Default passive traits (supports limit, in-process is kNA, no GC
  //   refs): constexpr base::MemoryConsumerTraits traits(
  //       base::MemoryConsumerTraits::ConsumerType::kPassive);
  //
  //   // Passive with custom optional traits:
  //   constexpr base::MemoryConsumerTraits traits(
  //       base::MemoryConsumerTraits::ConsumerType::kPassive,
  //       base::MemoryConsumerTraits::InProcess::kYes,
  //       base::MemoryConsumerTraits::ReleaseGCReferences::kYes);
  template <typename... Args>
    requires trait_helpers::AreValidTraits<PassiveOptionalTraitsList, Args...>
  constexpr explicit MemoryConsumerTraits(ConsumerType consumer_type,
                                          Args... args)
      : consumer_type(ConsumerType::kPassive),
        estimated_memory_usage(EstimatedMemoryUsage::kNA),
        release_memory_cost(ReleaseMemoryCost::kNA),
        information_retention(InformationRetention::kNA),
        execution_type(ExecutionType::kSynchronous),
        supports_memory_limit(
            internal::GetTraitOrCustomDefault<SupportsMemoryLimit,
                                              SupportsMemoryLimit::kYes>(
                args...)),
        in_process(internal::GetTraitOrCustomDefault<InProcess, InProcess::kNA>(
            args...)),
        recreate_memory_cost(RecreateMemoryCost::kNA),
        release_gc_references(
            internal::GetTraitOrCustomDefault<ReleaseGCReferences,
                                              ReleaseGCReferences::kNo>(
                args...)),
        garbage_collects_v8_heap(GarbageCollectsV8Heap::kNo),
        is_stateful(IsStateful::kYes),
        recovery_behavior(RecoveryBehavior::kNA) {
    CHECK_EQ(consumer_type, ConsumerType::kPassive);
  }

  // Constructs a MemoryConsumerTraits for an active consumer with the specified
  // required traits and zero or more optional traits. Uses the default value
  // for optional traits that are not specified.
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
      : consumer_type(internal::GetTraitOrDefault<ConsumerType>(args...)),
        estimated_memory_usage(estimated_memory_usage),
        release_memory_cost(release_memory_cost),
        information_retention(information_retention),
        execution_type(execution_type),
        supports_memory_limit(
            internal::GetTraitOrDefault<SupportsMemoryLimit>(args...)),
        in_process(internal::GetTraitOrDefault<InProcess>(args...)),
        recreate_memory_cost(
            internal::GetTraitOrDefault<RecreateMemoryCost>(args...)),
        release_gc_references(
            internal::GetTraitOrDefault<ReleaseGCReferences>(args...)),
        garbage_collects_v8_heap(
            internal::GetTraitOrDefault<GarbageCollectsV8Heap>(args...)),
        is_stateful(internal::GetTraitOrDefault<IsStateful>(args...)),
        recovery_behavior(
            internal::GetTraitOrDefault<RecoveryBehavior>(args...)) {
    CHECK_EQ(consumer_type, ConsumerType::kActive);
  }

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
  ConsumerType consumer_type;
  EstimatedMemoryUsage estimated_memory_usage;
  ReleaseMemoryCost release_memory_cost;
  InformationRetention information_retention;
  ExecutionType execution_type;
  SupportsMemoryLimit supports_memory_limit;
  InProcess in_process;
  RecreateMemoryCost recreate_memory_cost;
  ReleaseGCReferences release_gc_references;
  GarbageCollectsV8Heap garbage_collects_v8_heap;
  IsStateful is_stateful;
  RecoveryBehavior recovery_behavior;
  // LINT.ThenChange(//content/common/memory_coordinator/mojom/memory_coordinator.mojom)
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_TRAITS_H_
