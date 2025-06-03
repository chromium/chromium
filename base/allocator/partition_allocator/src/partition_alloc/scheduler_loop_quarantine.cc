// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/scheduler_loop_quarantine.h"

#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "partition_alloc/thread_cache.h"

namespace partition_alloc::internal {

namespace {
// Utility to disable thread safety analysis when we know it is safe.
class PA_SCOPED_LOCKABLE FakeScopedGuard {
 public:
  PA_ALWAYS_INLINE explicit FakeScopedGuard(Lock& lock)
      PA_EXCLUSIVE_LOCK_FUNCTION(lock) {}
  // For some reason, defaulting this causes a thread safety annotation failure.
  PA_ALWAYS_INLINE
  ~FakeScopedGuard()  // NOLINT(modernize-use-equals-default)
      PA_UNLOCK_FUNCTION() {}
};

// Utility classes to lock only if a condition is met.
template <bool thread_bound>
using ScopedGuardIfNeeded =
    std::conditional_t<thread_bound, FakeScopedGuard, ScopedGuard>;
}  // namespace

template <bool thread_bound>
SchedulerLoopQuarantineBranch<thread_bound>::SchedulerLoopQuarantineBranch(
    PartitionRoot* allocator_root,
    ThreadCache* tcache)
    : allocator_root_(allocator_root), tcache_(tcache) {
  PA_CHECK(allocator_root);
  if constexpr (kThreadBound) {
    PA_CHECK(tcache_);
  } else {
    PA_CHECK(!tcache_);
  }
}

template <bool thread_bound>
SchedulerLoopQuarantineBranch<thread_bound>::~SchedulerLoopQuarantineBranch() {
  if (!leak_on_destruction_) {
    Purge();
  }
  ToBeFreedArray* buffer =
      to_be_freed_working_memory_.exchange(nullptr, std::memory_order_relaxed);
  if (buffer) {
    DestroyAtInternalPartition(buffer);
  }
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::Configure(
    SchedulerLoopQuarantineRoot& root,
    const SchedulerLoopQuarantineConfig& config) {
  PA_CHECK(pause_quarantine_ == 0);
  PA_CHECK(allocator_root_ == &root.allocator_root_);
  if constexpr (kThreadBound) {
    PA_CHECK(tcache_->GetRoot() == &root.allocator_root_);
  }

  ScopedGuardIfNeeded<kThreadBound> guard(lock_);
  config_for_testing_ = config;

  if (enable_quarantine_) {
    // Already enabled, explicitly purging an existing instance.
    PurgeInternal(0);
    PA_CHECK(slots_.empty());
    slots_.shrink_to_fit();
  }

  root_ = &root;
  enable_quarantine_ = config.enable_quarantine;
  enable_zapping_ = config.enable_zapping;
  leak_on_destruction_ = config.leak_on_destruction;
  branch_capacity_in_bytes_ = config.branch_capacity_in_bytes;

  std::unique_ptr<ToBeFreedArray, InternalPartitionDeleter<ToBeFreedArray>>
      to_be_freed;
  if (enable_quarantine_ && !kThreadBound) {
    // Create a new buffer and delete an old one if exists.
    to_be_freed.reset(to_be_freed_working_memory_.exchange(
        ConstructAtInternalPartition<ToBeFreedArray>()));
  } else {
    // Just delete an old buffer if exists.
    to_be_freed.reset(to_be_freed_working_memory_.exchange(nullptr));
  }
}

template <bool thread_bound>
bool SchedulerLoopQuarantineBranch<thread_bound>::IsQuarantinedForTesting(
    void* object) {
  ScopedGuardIfNeeded<kThreadBound> guard(lock_);
  uintptr_t slot_start = allocator_root_->ObjectToSlotStartUnchecked(object);
  for (const auto& slot : slots_) {
    if (slot.slot_start == slot_start) {
      return true;
    }
  }
  return false;
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::SetCapacityInBytes(
    size_t capacity_in_bytes) {
  branch_capacity_in_bytes_.store(capacity_in_bytes, std::memory_order_relaxed);
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::Purge() {
  ScopedGuardIfNeeded<kThreadBound> guard(lock_);
  PurgeInternal(0);
  slots_.shrink_to_fit();
}

template <>
PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
void SchedulerLoopQuarantineBranch<false>::Quarantine(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size) {
  if (!enable_quarantine_ || pause_quarantine_ ||
      allocator_root_->IsDirectMappedBucket(slot_span->bucket)) [[unlikely]] {
    return allocator_root_->FreeNoHooksImmediate(object, slot_span, slot_start);
  }

  PA_DCHECK(usable_size == allocator_root_->GetSlotUsableSize(slot_span));

  const size_t capacity_in_bytes =
      branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < usable_size) [[unlikely]] {
    // Even if this branch dequarantines all entries held by it, this entry
    // cannot fit within the capacity.
    allocator_root_->FreeNoHooksImmediate(object, slot_span, slot_start);
    root_->quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
    return;
  }

  std::unique_ptr<ToBeFreedArray, InternalPartitionDeleter<ToBeFreedArray>>
      to_be_freed;
  size_t num_of_slots = 0;

  // Borrow the reserved working memory from to_be_freed_working_memory_,
  // and set nullptr to it indicating that it's in use.
  to_be_freed.reset(to_be_freed_working_memory_.exchange(nullptr));
  if (!to_be_freed) {
    // When the reserved working memory has already been in use by another
    // thread, fall back to allocate another chunk of working memory.
    to_be_freed.reset(ConstructAtInternalPartition<ToBeFreedArray>());
  }

  {
    ScopedGuardIfNeeded<kThreadBound> guard(lock_);

    // Dequarantine some entries as required. Save the objects to be
    // deallocated into `to_be_freed`.
    PurgeInternalWithDefferedFree(capacity_in_bytes - usable_size, *to_be_freed,
                                  num_of_slots);

    // Put the entry onto the list.
    branch_size_in_bytes_ += usable_size;
    slots_.push_back({slot_start, usable_size});

    // Swap randomly so that the quarantine list remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % slots_.size();
    std::swap(slots_[random_index], slots_.back());
  }

  // Actually deallocate the dequarantined objects.
  BatchFree(*to_be_freed, num_of_slots);

  // Return the possibly-borrowed working memory to
  // to_be_freed_working_memory_. It doesn't matter much if it's really
  // borrowed or locally-allocated. The important facts are 1) to_be_freed is
  // non-null, and 2) to_be_freed_working_memory_ may likely be null (because
  // this or another thread has already borrowed it). It's simply good to make
  // to_be_freed_working_memory_ non-null whenever possible. Maybe yet another
  // thread would be about to borrow the working memory.
  to_be_freed.reset(
      to_be_freed_working_memory_.exchange(to_be_freed.release()));

  // Update stats (not locked).
  root_->count_.fetch_add(1, std::memory_order_relaxed);
  root_->size_in_bytes_.fetch_add(usable_size, std::memory_order_relaxed);
  root_->cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_->cumulative_size_in_bytes_.fetch_add(usable_size,
                                             std::memory_order_relaxed);

  QuarantineEpilogue(object, slot_span, slot_start, usable_size);
}

template <>
PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
void SchedulerLoopQuarantineBranch<true>::Quarantine(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size) {
  if (!enable_quarantine_ || pause_quarantine_ ||
      allocator_root_->IsDirectMappedBucket(slot_span->bucket)) [[unlikely]] {
    return allocator_root_->FreeNoHooksImmediate(object, slot_span, slot_start);
  }

  PA_DCHECK(usable_size == allocator_root_->GetSlotUsableSize(slot_span));

  const size_t capacity_in_bytes =
      branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < usable_size) [[unlikely]] {
    // Even if this branch dequarantines all entries held by it, this entry
    // cannot fit within the capacity.
    allocator_root_->FreeNoHooksImmediate(object, slot_span, slot_start);
    root_->quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
    return;
  }

  ScopedGuardIfNeeded<kThreadBound> guard(lock_);

  // Dequarantine some entries as required.
  PurgeInternal(capacity_in_bytes - usable_size);

  // Put the entry onto the list.
  branch_size_in_bytes_ += usable_size;
  slots_.push_back({slot_start, usable_size});

  // Swap randomly so that the quarantine list remain shuffled.
  // This is not uniformly random, but sufficiently random.
  const size_t random_index = random_.RandUint32() % slots_.size();
  std::swap(slots_[random_index], slots_.back());

  // Update stats (not locked).
  root_->count_.fetch_add(1, std::memory_order_relaxed);
  root_->size_in_bytes_.fetch_add(usable_size, std::memory_order_relaxed);
  root_->cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_->cumulative_size_in_bytes_.fetch_add(usable_size,
                                             std::memory_order_relaxed);

  QuarantineEpilogue(object, slot_span, slot_start, usable_size);
}

template <bool thread_bound>
PA_ALWAYS_INLINE void
SchedulerLoopQuarantineBranch<thread_bound>::PurgeInternal(
    size_t target_size_in_bytes) {
  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    PA_DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    const auto& to_free = slots_.back();
    size_t to_free_size = to_free.usable_size;

    auto* slot_span = SlotSpanMetadata<MetadataKind::kReadOnly>::FromSlotStart(
        to_free.slot_start);
    void* object = allocator_root_->SlotStartToObject(to_free.slot_start);
    PA_DCHECK(slot_span ==
              SlotSpanMetadata<MetadataKind::kReadOnly>::FromObject(object));

    PA_DCHECK(to_free.slot_start);
    allocator_root_->FreeNoHooksImmediate(object, slot_span,
                                          to_free.slot_start);

    freed_count++;
    freed_size_in_bytes += to_free_size;
    branch_size_in_bytes_ -= to_free_size;

    slots_.pop_back();
  }

  root_->size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                  std::memory_order_relaxed);
  root_->count_.fetch_sub(freed_count, std::memory_order_relaxed);
}

template <bool thread_bound>
PA_ALWAYS_INLINE void
SchedulerLoopQuarantineBranch<thread_bound>::PurgeInternalWithDefferedFree(
    size_t target_size_in_bytes,
    ToBeFreedArray& to_be_freed,
    size_t& num_of_slots) {
  num_of_slots = 0;

  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    PA_DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent to
    // picking random entry.
    const QuarantineSlot& to_free = slots_.back();
    const size_t to_free_size = to_free.usable_size;

    to_be_freed[num_of_slots++] = to_free.slot_start;
    slots_.pop_back();

    freed_size_in_bytes += to_free_size;
    branch_size_in_bytes_ -= to_free_size;

    if (num_of_slots >= kMaxFreeTimesPerPurge) {
      break;
    }
  }

  root_->size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                  std::memory_order_relaxed);
  root_->count_.fetch_sub(num_of_slots, std::memory_order_relaxed);
}

template <bool thread_bound>
PA_ALWAYS_INLINE void SchedulerLoopQuarantineBranch<thread_bound>::BatchFree(
    const ToBeFreedArray& to_be_freed,
    size_t num_of_slots) {
  for (size_t i = 0; i < num_of_slots; ++i) {
    const uintptr_t slot_start = to_be_freed[i];
    PA_DCHECK(slot_start);
    auto* slot_span =
        SlotSpanMetadata<MetadataKind::kReadOnly>::FromSlotStart(slot_start);
    void* object = allocator_root_->SlotStartToObject(slot_start);
    PA_DCHECK(slot_span ==
              SlotSpanMetadata<MetadataKind::kReadOnly>::FromObject(object));
    allocator_root_->FreeNoHooksImmediate(object, slot_span, slot_start);
  }
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::QuarantineEpilogue(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size) {
  if (enable_zapping_) {
    internal::SecureMemset(object, internal::kFreedByte, usable_size);
  }

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  // TODO(keishi): Add `[[likely]]` when brp is fully enabled as
  // `brp_enabled` will be false only for the aligned partition.
  if (allocator_root_->brp_enabled()) {
    auto* ref_count = PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
        slot_start, slot_span->bucket->slot_size);
    ref_count->PreReleaseFromAllocator();
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
}

template <bool thread_bound>
const SchedulerLoopQuarantineConfig&
SchedulerLoopQuarantineBranch<thread_bound>::GetConfigurationForTesting() {
  return config_for_testing_;
}

template class PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
    SchedulerLoopQuarantineBranch<false>;
template class PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
    SchedulerLoopQuarantineBranch<true>;

}  // namespace partition_alloc::internal
