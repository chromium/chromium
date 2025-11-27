// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/scheduler_loop_quarantine.h"

#include <atomic>

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

// When set to `true`, all the branches stop purging. It helps to reduce
// shutdown hangs.
std::atomic_bool g_no_purge = false;

}  // namespace

// Utility class to process batched-free operation.
class BatchFreeQueue {
 public:
  PA_ALWAYS_INLINE explicit BatchFreeQueue(PartitionRoot* root) : root_(root) {}
  PA_ALWAYS_INLINE ~BatchFreeQueue() { Purge(); }

  PA_ALWAYS_INLINE void Queue(UntaggedSlotStart slot_start) {
    auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start, root_);

    // Direct-mapped deallocation releases then re-acquires the lock. The caller
    // may not expect that, but we never call this function on direct-mapped
    // allocations.
    PA_DCHECK(!root_->IsDirectMapped(slot_span));

    Entry& entry = queue_[size_];
    ++size_;

    entry = {
        .slot_start = slot_start,
        .slot_span = slot_span,
    };

    if (size_ == kQueueSize) {
      Purge();
    }
  }

  PA_ALWAYS_INLINE void Purge() {
    if (!size_) {
      return;
    }

    for (size_t i = 0; i < size_; ++i) {
      Entry& entry = queue_[i];

      // Make sure that we fault *before* locking. See `PartitionRoot::RawFree`
      // for detailed performance reasons.
      auto* object = entry.slot_start.Tag().ToObject<volatile uintptr_t>();
      *object = 0;

      // Also we are going to write into |*slot_span|.
      PA_PREFETCH_FOR_WRITE(entry.slot_span);
    }

    internal::ScopedGuard guard(internal::PartitionRootLock(root_));
    do {
      --size_;
      Entry& entry = queue_[size_];

      root_->RawFreeLocked(entry.slot_start, entry.slot_span);
    } while (size_);
  }

 private:
  struct Entry {
    UntaggedSlotStart slot_start;
    SlotSpanMetadata* slot_span;
  };

  PartitionRoot* const root_;
  size_t size_ = 0;

  constexpr static size_t kQueueSize = 16;
  std::array<Entry, kQueueSize> queue_;
};

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

  // This bucket index can be invalid if "Neutral" distribution is in use,
  // but value here is only for comparison and should be safe.
  largest_bucket_index_ =
      BucketIndexLookup::GetIndexForDenserBuckets(config.max_quarantine_size);
  PA_CHECK(largest_bucket_index_ < BucketIndexLookup::kNumBuckets);
  PA_CHECK(&allocator_root_->buckets[largest_bucket_index_] <=
           &allocator_root_->sentinel_bucket);
}

template <bool thread_bound>
bool SchedulerLoopQuarantineBranch<thread_bound>::IsQuarantinedForTesting(
    void* object) {
  ScopedGuardIfNeeded<kThreadBound> guard(lock_);
  UntaggedSlotStart slot_start = SlotStart::Unchecked(object).Untag();
  for (const auto& slot : slots_) {
    if (slot.slot_start.Untag() == slot_start) {
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
    SlotStart slot_start,
    SlotSpanMetadata* slot_span) {
  auto size_details = allocator_root_->SlotSpanToBucketSizeDetails(slot_span);
  return QuarantineWithSize(slot_start, slot_span, size_details);
}

template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::QuarantineWithSize(
    SlotStart slot_start,
    SlotSpanMetadata* slot_span,
    const internal::BucketSizeDetails& size_details) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_DCHECK(!being_destructed_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  if (!enable_quarantine_ || pause_quarantine_) [[unlikely]] {
    return allocator_root_->RawFreeWithThreadCacheWithSize(
        slot_start, size_details, slot_span);
  }
  if (size_details.slot_size > BucketIndexLookup::kMaxBucketSize ||
      largest_bucket_index_ < size_details.bucket_index) [[unlikely]] {
    // The allocation is direct-mapped or larger than `largest_bucket_index_`.
    return allocator_root_->RawFreeWithThreadCacheWithSize(
        slot_start, size_details, slot_span);
  }
  PA_DCHECK(!allocator_root_->IsDirectMapped(slot_span));
  PA_DCHECK(slot_span->bucket >= &allocator_root_->buckets[0] &&
            slot_span->bucket <=
                &allocator_root_->buckets[largest_bucket_index_]);

  const size_t slot_size = size_details.slot_size;
  const size_t capacity_in_bytes =
      branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < slot_size) [[unlikely]] {
    // Even if this branch dequarantines all entries held by it, this entry
    // cannot fit within the capacity.
    allocator_root_->RawFreeWithThreadCacheWithSize(slot_start, size_details,
                                                    slot_span);
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
      .bucket_index = size_details.bucket_index,
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
    internal::SecureMemset(slot_start.ToObject(), internal::kFreedByte,
                           slot_size);
  }
}

template <bool thread_bound>
PA_ALWAYS_INLINE void
SchedulerLoopQuarantineBranch<thread_bound>::PurgeInternal(
    size_t target_size_in_bytes,
    [[maybe_unused]] bool for_destruction) {
  if (g_no_purge.load(std::memory_order_relaxed)) {
    return;
  }

  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  BatchFreeQueue queue(allocator_root_);

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    PA_DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    SlotStart slot_start = slots_.back().slot_start;
    const size_t bucket_index = slots_.back().bucket_index;
    size_t slot_size = 0;

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    allocator_root_->RetagSlotIfNeeded(slot_start.Untag(), slot_size);
    slot_start = slot_start.Untag().Tag();
#endif
    if constexpr (!kThreadBound) {
      // Assuming that ThreadCache is not available as this is not thread-bound.
      // Going to `RawFree()` directly.
      slot_size = BucketIndexLookup::GetBucketSize(bucket_index);
      queue.Queue(slot_start.Untag());
    } else {
      // Unless during its destruction, we can assume ThreadCache is valid
      // because this branch is embedded inside ThreadCache.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      PA_DCHECK(being_destructed_ || ThreadCache::IsValid(ThreadCache::Get()));
      PA_DCHECK(being_destructed_ || ThreadCache::Get() == tcache_);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

      std::optional<size_t> slot_size_opt =
          tcache_->MaybePutInCache(slot_start.Untag(), bucket_index);

      if (slot_size_opt.has_value()) [[likely]] {
        slot_size = slot_size_opt.value();
        // This is a fast path, avoid calling GetSlotUsableSize() in Release
        // builds as it is costlier. Copy its small bucket path instead.
        const size_t usable_size =
            allocator_root_->AdjustSizeForExtrasSubtract(slot_size);

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
        auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start.Untag(),
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
        const size_t usable_size =
            allocator_root_->AdjustSizeForExtrasSubtract(slot_size);
        tcache_->RecordDeallocation(usable_size);
        queue.Queue(slot_start.Untag());
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

// static
template <bool thread_bound>
void SchedulerLoopQuarantineBranch<thread_bound>::DangerouslyDisablePurge() {
  g_no_purge.store(true, std::memory_order_relaxed);
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
