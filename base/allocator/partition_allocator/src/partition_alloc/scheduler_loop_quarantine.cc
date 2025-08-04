// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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
  Destroy();
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
  PA_DCHECK(slots_.capacity() == 0);
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::Destroy() {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  being_destructed_ = true;
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  if (!leak_on_destruction_) {
    Purge();
  }
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::Quarantine(
    void* object,
    SlotSpanMetadata* slot_span,
    uintptr_t slot_start) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(!being_destructed_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  if (!enable_quarantine_ || pause_quarantine_ ||
      allocator_root_->IsDirectMappedBucket(slot_span->bucket)) [[unlikely]] {
    return allocator_root_->RawFreeWithThreadCache(slot_start, object,
                                                   slot_span);
  }

  const size_t slot_size = slot_span->bucket->slot_size;
  const size_t bucket_index =
      static_cast<size_t>(slot_span->bucket - allocator_root_->buckets);
  const size_t capacity_in_bytes =
      branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < slot_size) [[unlikely]] {
    // Even if this branch dequarantines all entries held by it, this entry
    // cannot fit within the capacity.
    allocator_root_->RawFreeWithThreadCache(slot_start, object, slot_span);
    root_->quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
    return;
  }

  ScopedGuardIfNeeded<kThreadBound> guard(lock_);

  // Dequarantine some entries as required.
  PurgeInternal(capacity_in_bytes - slot_size);

  // Put the entry onto the list.
  branch_size_in_bytes_ += slot_size;
  slots_.push_back({
      .slot_start = slot_start,
      .bucket_index = bucket_index,
  });

  // Swap randomly so that the quarantine list remain shuffled.
  // This is not uniformly random, but sufficiently random.
  const size_t random_index = random_.RandUint32() % slots_.size();
  std::swap(slots_[random_index], slots_.back());

  // Update stats (not locked).
  root_->count_.fetch_add(1, std::memory_order_relaxed);
  root_->size_in_bytes_.fetch_add(slot_size, std::memory_order_relaxed);
  root_->cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_->cumulative_size_in_bytes_.fetch_add(slot_size,
                                             std::memory_order_relaxed);

  if (enable_zapping_) {
    internal::SecureMemset(object, internal::kFreedByte, slot_size);
  }
}

template <bool thread_bound>
PA_ALWAYS_INLINE void
SchedulerLoopQuarantineBranch<thread_bound>::PurgeInternal(
    size_t target_size_in_bytes,
    [[maybe_unused]] bool for_destruction) {
  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    PA_DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    const auto& to_free = slots_.back();
    const size_t bucket_index = to_free.bucket_index;
    size_t slot_size = 0;

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    allocator_root_->RetagSlotIfNeeded(SlotStartAddr2Ptr(to_free.slot_start),
                                       slot_size);
#endif
    if constexpr (!kThreadBound) {
      // Assuming that ThreadCache is not available as this is not thread-bound.
      // Going to `RawFree()` directly.
      slot_size = BucketIndexLookup::GetBucketSize(bucket_index);
      auto* slot_span =
          SlotSpanMetadata::FromSlotStart(to_free.slot_start, allocator_root_);
      allocator_root_->RawFree(to_free.slot_start, slot_span);
    } else {
      // Unless during its destruction, we can assume ThreadCache is valid
      // because this branch is embedded inside ThreadCache.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      PA_DCHECK(being_destructed_ || ThreadCache::IsValid(ThreadCache::Get()));
      PA_DCHECK(being_destructed_ || ThreadCache::Get() == tcache_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

      std::optional<size_t> slot_size_opt =
          tcache_->MaybePutInCache(to_free.slot_start, bucket_index);

      if (slot_size_opt.has_value()) [[likely]] {
        slot_size = slot_size_opt.value();
        // This is a fast path, avoid calling GetSlotUsableSize() in Release
        // builds as it is costlier. Copy its small bucket path instead.
        const size_t usable_size =
            allocator_root_->AdjustSizeForExtrasSubtract(slot_size);

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
        auto* slot_span = SlotSpanMetadata::FromSlotStart(to_free.slot_start,
                                                          allocator_root_);
        PA_DCHECK(!slot_span->CanStoreRawSize());
        PA_DCHECK(usable_size == allocator_root_->GetSlotUsableSize(slot_span));
#endif
        tcache_->RecordDeallocation(usable_size);
        // Now ThreadCache is responsible for freeing the allocation.
      } else {
        // ThreadCache refused to take ownership of the allocation, hence we
        // free it.
        slot_size = BucketIndexLookup::GetBucketSize(bucket_index);
        auto* slot_span = SlotSpanMetadata::FromSlotStart(to_free.slot_start,
                                                          allocator_root_);
        size_t usable_size = allocator_root_->GetSlotUsableSize(slot_span);
        tcache_->RecordDeallocation(usable_size);
        allocator_root_->RawFree(to_free.slot_start, slot_span);
      }
    }

    ++freed_count;
    PA_DCHECK(slot_size > 0);
    freed_size_in_bytes += slot_size;
    branch_size_in_bytes_ -= slot_size;

    slots_.pop_back();
  }

  root_->size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                  std::memory_order_relaxed);
  root_->count_.fetch_sub(freed_count, std::memory_order_relaxed);
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::AllowScanlessPurge() {
  PA_DCHECK(kThreadBound);
  // Always thread-bound; no need to lock.
  FakeScopedGuard guard(lock_);

  PA_CHECK(disallow_scanless_purge_ > 0);
  --disallow_scanless_purge_;
  if (disallow_scanless_purge_ == 0) {
    // Now scanless purge is allowed. Purging at this timing is more performance
    // efficient.
    PurgeInternal(0);
  }
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::DisallowScanlessPurge() {
  PA_DCHECK(kThreadBound);
  // Always thread-bound; no need to lock.
  FakeScopedGuard guard(lock_);

  ++disallow_scanless_purge_;
  PA_CHECK(disallow_scanless_purge_ > 0);  // Overflow check.
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
