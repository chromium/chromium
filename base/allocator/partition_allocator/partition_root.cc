// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_root.h"

#include <cstdint>

#include "base/allocator/partition_allocator/freeslot_bitmap.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_base/thread_annotations.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_bucket.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/tagging.h"
#include "base/allocator/partition_allocator/thread_isolation/thread_isolation.h"
#include "build/build_config.h"

#if PA_CONFIG(ENABLE_MAC11_MALLOC_SIZE_HACK)
#include "base/allocator/partition_allocator/partition_alloc_base/mac/mac_util.h"
#endif

#if BUILDFLAG(USE_STARSCAN)
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#endif

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
#include "base/allocator/partition_allocator/address_pool_manager_bitmap.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "wow64apiset.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>
#endif

namespace partition_alloc::internal {

#if BUILDFLAG(RECORD_ALLOC_INFO)
// Even if this is not hidden behind a BUILDFLAG, it should not use any memory
// when recording is disabled, since it ends up in the .bss section.
AllocInfo g_allocs = {};

void RecordAllocOrFree(uintptr_t addr, size_t size) {
  g_allocs.allocs[g_allocs.index.fetch_add(1, std::memory_order_relaxed) %
                  kAllocInfoSize] = {addr, size};
}
#endif  // BUILDFLAG(RECORD_ALLOC_INFO)

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
PtrPosWithinAlloc IsPtrWithinSameAlloc(uintptr_t orig_address,
                                       uintptr_t test_address,
                                       size_t type_size) {
  // Required for pointers right past an allocation. See
  // |PartitionAllocGetSlotStartInBRPPool()|.
  uintptr_t adjusted_address =
      orig_address - kPartitionPastAllocationAdjustment;
  PA_DCHECK(IsManagedByNormalBucketsOrDirectMap(adjusted_address));
  DCheckIfManagedByPartitionAllocBRPPool(adjusted_address);

  uintptr_t slot_start = PartitionAllocGetSlotStartInBRPPool(adjusted_address);
  // Don't use |adjusted_address| beyond this point at all. It was needed to
  // pick the right slot, but now we're dealing with very concrete addresses.
  // Zero it just in case, to catch errors.
  adjusted_address = 0;

  auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotStart(slot_start);
  auto* root = PartitionRoot<ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->brp_enabled());

  uintptr_t object_addr = root->SlotStartToObjectAddr(slot_start);
  uintptr_t object_end = object_addr + slot_span->GetUsableSize(root);
  if (test_address < object_addr || object_end < test_address) {
    return PtrPosWithinAlloc::kFarOOB;
#if BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  } else if (object_end - type_size < test_address) {
    // Not even a single element of the type referenced by the pointer can fit
    // between the pointer and the end of the object.
    return PtrPosWithinAlloc::kAllocEnd;
#endif
  } else {
    return PtrPosWithinAlloc::kInBounds;
  }
}
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

}  // namespace partition_alloc::internal

namespace partition_alloc {

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)

namespace {
internal::Lock g_root_enumerator_lock;
}

template <bool thread_safe>
internal::Lock& PartitionRoot<thread_safe>::GetEnumeratorLock() {
  return g_root_enumerator_lock;
}

namespace internal {

class PartitionRootEnumerator {
 public:
  using EnumerateCallback = void (*)(ThreadSafePartitionRoot* root,
                                     bool in_child);
  enum EnumerateOrder {
    kNormal,
    kReverse,
  };

  static PartitionRootEnumerator& Instance() {
    static PartitionRootEnumerator instance;
    return instance;
  }

  void Enumerate(EnumerateCallback callback,
                 bool in_child,
                 EnumerateOrder order) PA_NO_THREAD_SAFETY_ANALYSIS {
    if (order == kNormal) {
      ThreadSafePartitionRoot* root;
      for (root = Head(partition_roots_); root != nullptr;
           root = root->next_root) {
        callback(root, in_child);
      }
    } else {
      PA_DCHECK(order == kReverse);
      ThreadSafePartitionRoot* root;
      for (root = Tail(partition_roots_); root != nullptr;
           root = root->prev_root) {
        callback(root, in_child);
      }
    }
  }

  void Register(ThreadSafePartitionRoot* root) {
    internal::ScopedGuard guard(ThreadSafePartitionRoot::GetEnumeratorLock());
    root->next_root = partition_roots_;
    root->prev_root = nullptr;
    if (partition_roots_) {
      partition_roots_->prev_root = root;
    }
    partition_roots_ = root;
  }

  void Unregister(ThreadSafePartitionRoot* root) {
    internal::ScopedGuard guard(ThreadSafePartitionRoot::GetEnumeratorLock());
    ThreadSafePartitionRoot* prev = root->prev_root;
    ThreadSafePartitionRoot* next = root->next_root;
    if (prev) {
      PA_DCHECK(prev->next_root == root);
      prev->next_root = next;
    } else {
      PA_DCHECK(partition_roots_ == root);
      partition_roots_ = next;
    }
    if (next) {
      PA_DCHECK(next->prev_root == root);
      next->prev_root = prev;
    }
    root->next_root = nullptr;
    root->prev_root = nullptr;
  }

 private:
  constexpr PartitionRootEnumerator() = default;

  ThreadSafePartitionRoot* Head(ThreadSafePartitionRoot* roots) {
    return roots;
  }

  ThreadSafePartitionRoot* Tail(ThreadSafePartitionRoot* roots)
      PA_NO_THREAD_SAFETY_ANALYSIS {
    if (!roots) {
      return nullptr;
    }
    ThreadSafePartitionRoot* node = roots;
    for (; node->next_root != nullptr; node = node->next_root)
      ;
    return node;
  }

  ThreadSafePartitionRoot* partition_roots_
      PA_GUARDED_BY(ThreadSafePartitionRoot::GetEnumeratorLock()) = nullptr;
};

}  // namespace internal

#endif  // PA_USE_PARTITION_ROOT_ENUMERATOR

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

#if PA_CONFIG(HAS_ATFORK_HANDLER)

void LockRoot(PartitionRoot<internal::ThreadSafe>* root,
              bool) PA_NO_THREAD_SAFETY_ANALYSIS {
  PA_DCHECK(root);
  root->lock_.Acquire();
}

// PA_NO_THREAD_SAFETY_ANALYSIS: acquires the lock and doesn't release it, by
// design.
void BeforeForkInParent() PA_NO_THREAD_SAFETY_ANALYSIS {
  // ThreadSafePartitionRoot::GetLock() is private. So use
  // g_root_enumerator_lock here.
  g_root_enumerator_lock.Acquire();
  internal::PartitionRootEnumerator::Instance().Enumerate(
      LockRoot, false,
      internal::PartitionRootEnumerator::EnumerateOrder::kNormal);

  ThreadCacheRegistry::GetLock().Acquire();
}

template <typename T>
void UnlockOrReinit(T& lock, bool in_child) PA_NO_THREAD_SAFETY_ANALYSIS {
  // Only re-init the locks in the child process, in the parent can unlock
  // normally.
  if (in_child) {
    lock.Reinit();
  } else {
    lock.Release();
  }
}

void UnlockOrReinitRoot(PartitionRoot<internal::ThreadSafe>* root,
                        bool in_child) PA_NO_THREAD_SAFETY_ANALYSIS {
  UnlockOrReinit(root->lock_, in_child);
}

void ReleaseLocks(bool in_child) PA_NO_THREAD_SAFETY_ANALYSIS {
  // In reverse order, even though there are no lock ordering dependencies.
  UnlockOrReinit(ThreadCacheRegistry::GetLock(), in_child);
  internal::PartitionRootEnumerator::Instance().Enumerate(
      UnlockOrReinitRoot, in_child,
      internal::PartitionRootEnumerator::EnumerateOrder::kReverse);

  // ThreadSafePartitionRoot::GetLock() is private. So use
  // g_root_enumerator_lock here.
  UnlockOrReinit(g_root_enumerator_lock, in_child);
}

void AfterForkInParent() {
  ReleaseLocks(/* in_child = */ false);
}

void AfterForkInChild() {
  ReleaseLocks(/* in_child = */ true);
  // Unsafe, as noted in the name. This is fine here however, since at this
  // point there is only one thread, this one (unless another post-fork()
  // handler created a thread, but it would have needed to allocate, which would
  // have deadlocked the process already).
  //
  // If we don't reclaim this memory, it is lost forever. Note that this is only
  // really an issue if we fork() a multi-threaded process without calling
  // exec() right away, which is discouraged.
  ThreadCacheRegistry::Instance().ForcePurgeAllThreadAfterForkUnsafe();
}
#endif  // PA_CONFIG(HAS_ATFORK_HANDLER)

std::atomic<bool> g_global_init_called;
void PartitionAllocMallocInitOnce() {
  bool expected = false;
  // No need to block execution for potential concurrent initialization, merely
  // want to make sure this is only called once.
  if (!g_global_init_called.compare_exchange_strong(expected, true)) {
    return;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // When fork() is called, only the current thread continues to execute in the
  // child process. If the lock is held, but *not* by this thread when fork() is
  // called, we have a deadlock.
  //
  // The "solution" here is to acquire the lock on the forking thread before
  // fork(), and keep it held until fork() is done, in the parent and the
  // child. To clean up memory, we also must empty the thread caches in the
  // child, which is easier, since no threads except for the current one are
  // running right after the fork().
  //
  // This is not perfect though, since:
  // - Multiple pre/post-fork() handlers can be registered, they are then run in
  //   LIFO order for the pre-fork handler, and FIFO order for the post-fork
  //   one. So unless we are the first to register a handler, if another handler
  //   allocates, then we deterministically deadlock.
  // - pthread handlers are *not* called when the application calls clone()
  //   directly, which is what Chrome does to launch processes.
  //
  // However, no perfect solution really exists to make threads + fork()
  // cooperate, but deadlocks are real (and fork() is used in DEATH_TEST()s),
  // and other malloc() implementations use the same techniques.
  int err =
      pthread_atfork(BeforeForkInParent, AfterForkInParent, AfterForkInChild);
  PA_CHECK(err == 0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

#if BUILDFLAG(IS_APPLE)
void PartitionAllocMallocHookOnBeforeForkInParent() {
  BeforeForkInParent();
}

void PartitionAllocMallocHookOnAfterForkInParent() {
  AfterForkInParent();
}

void PartitionAllocMallocHookOnAfterForkInChild() {
  AfterForkInChild();
}
#endif  // BUILDFLAG(IS_APPLE)

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace internal {

namespace {
// 64 was chosen arbitrarily, as it seems like a reasonable trade-off between
// performance and purging opportunity. Higher value (i.e. smaller slots)
// wouldn't necessarily increase chances of purging, but would result in
// more work and larger |slot_usage| array. Lower value would probably decrease
// chances of purging. Not empirically tested.
constexpr size_t kMaxPurgeableSlotsPerSystemPage = 64;
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MinPurgeableSlotSize() {
  return SystemPageSize() / kMaxPurgeableSlotsPerSystemPage;
}
}  // namespace

template <bool thread_safe>
static size_t PartitionPurgeSlotSpan(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    bool discard) {
  auto* root = PartitionRoot<thread_safe>::FromSlotSpan(slot_span);
  const internal::PartitionBucket<thread_safe>* bucket = slot_span->bucket;
  size_t slot_size = bucket->slot_size;

  if (slot_size < MinPurgeableSlotSize() || !slot_span->num_allocated_slots) {
    return 0;
  }

  size_t bucket_num_slots = bucket->get_slots_per_span();
  size_t discardable_bytes = 0;

  if (slot_span->CanStoreRawSize()) {
    uint32_t utilized_slot_size = static_cast<uint32_t>(
        RoundUpToSystemPage(slot_span->GetUtilizedSlotSize()));
    discardable_bytes = bucket->slot_size - utilized_slot_size;
    if (discardable_bytes && discard) {
      uintptr_t slot_span_start =
          internal::SlotSpanMetadata<thread_safe>::ToSlotSpanStart(slot_span);
      uintptr_t committed_data_end = slot_span_start + utilized_slot_size;
      ScopedSyscallTimer timer{root};
      DiscardSystemPages(committed_data_end, discardable_bytes);
    }
    return discardable_bytes;
  }

#if defined(PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR)
  constexpr size_t kMaxSlotCount =
      (PartitionPageSize() * kMaxPartitionPagesPerRegularSlotSpan) /
      MinPurgeableSlotSize();
#elif BUILDFLAG(IS_APPLE) || (BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64))
  // It's better for slot_usage to be stack-allocated and fixed-size, which
  // demands that its size be constexpr. On IS_APPLE and Linux on arm64,
  // PartitionPageSize() is always SystemPageSize() << 2, so regardless of
  // what the run time page size is, kMaxSlotCount can always be simplified
  // to this expression.
  constexpr size_t kMaxSlotCount =
      4 * kMaxPurgeableSlotsPerSystemPage *
      internal::kMaxPartitionPagesPerRegularSlotSpan;
  PA_CHECK(kMaxSlotCount == (PartitionPageSize() *
                             internal::kMaxPartitionPagesPerRegularSlotSpan) /
                                MinPurgeableSlotSize());
#endif
  PA_DCHECK(bucket_num_slots <= kMaxSlotCount);
  PA_DCHECK(slot_span->num_unprovisioned_slots < bucket_num_slots);
  size_t num_slots = bucket_num_slots - slot_span->num_unprovisioned_slots;
  char slot_usage[kMaxSlotCount];
#if !BUILDFLAG(IS_WIN)
  // The last freelist entry should not be discarded when using OS_WIN.
  // DiscardVirtualMemory makes the contents of discarded memory undefined.
  size_t last_slot = static_cast<size_t>(-1);
#endif
  memset(slot_usage, 1, num_slots);
  uintptr_t slot_span_start =
      SlotSpanMetadata<thread_safe>::ToSlotSpanStart(slot_span);
  // First, walk the freelist for this slot span and make a bitmap of which
  // slots are not in use.
  for (PartitionFreelistEntry* entry = slot_span->get_freelist_head(); entry;
       /**/) {
    size_t slot_number =
        bucket->GetSlotNumber(SlotStartPtr2Addr(entry) - slot_span_start);
    PA_DCHECK(slot_number < num_slots);
    slot_usage[slot_number] = 0;
#if !BUILDFLAG(IS_WIN)
    // If we have a slot where the encoded next pointer is 0, we can actually
    // discard that entry because touching a discarded page is guaranteed to
    // return the original content or 0. (Note that this optimization won't be
    // effective on big-endian machines because the masking function is
    // negation.)
    if (entry->IsEncodedNextPtrZero()) {
      last_slot = slot_number;
    }
#endif
    entry = entry->GetNext(slot_size);
  }

  // If the slot(s) at the end of the slot span are not in used, we can truncate
  // them entirely and rewrite the freelist.
  size_t truncated_slots = 0;
  while (!slot_usage[num_slots - 1]) {
    truncated_slots++;
    num_slots--;
    PA_DCHECK(num_slots);
  }
  // First, do the work of calculating the discardable bytes. Don't actually
  // discard anything unless the discard flag was passed in.
  if (truncated_slots) {
    size_t unprovisioned_bytes = 0;
    uintptr_t begin_addr = slot_span_start + (num_slots * slot_size);
    uintptr_t end_addr = begin_addr + (slot_size * truncated_slots);

    // The slots that do not contain discarded pages should not be included to
    // |truncated_slots|. Detects those slots and fixes |truncated_slots| and
    // |num_slots| accordingly.
    uintptr_t rounded_up_truncatation_begin_addr =
        RoundUpToSystemPage(begin_addr);
    while (begin_addr + slot_size <= rounded_up_truncatation_begin_addr) {
      begin_addr += slot_size;
      PA_DCHECK(truncated_slots);
      --truncated_slots;
      ++num_slots;
    }
    begin_addr = rounded_up_truncatation_begin_addr;

    // We round the end address here up and not down because we're at the end of
    // a slot span, so we "own" all the way up the page boundary.
    end_addr = RoundUpToSystemPage(end_addr);
    PA_DCHECK(end_addr <= slot_span_start + bucket->get_bytes_per_span());
    if (begin_addr < end_addr) {
      unprovisioned_bytes = end_addr - begin_addr;
      discardable_bytes += unprovisioned_bytes;
    }
    if (unprovisioned_bytes && discard) {
      PA_DCHECK(truncated_slots > 0);
      size_t new_unprovisioned_slots =
          truncated_slots + slot_span->num_unprovisioned_slots;
      PA_DCHECK(new_unprovisioned_slots <= bucket->get_slots_per_span());
      slot_span->num_unprovisioned_slots = new_unprovisioned_slots;

      // Rewrite the freelist.
      internal::PartitionFreelistEntry* head = nullptr;
      internal::PartitionFreelistEntry* back = head;
      size_t num_new_entries = 0;
      for (size_t slot_index = 0; slot_index < num_slots; ++slot_index) {
        if (slot_usage[slot_index]) {
          continue;
        }

        auto* entry = PartitionFreelistEntry::EmplaceAndInitNull(
            slot_span_start + (slot_size * slot_index));
        if (!head) {
          head = entry;
          back = entry;
        } else {
          back->SetNext(entry);
          back = entry;
        }
        num_new_entries++;
#if !BUILDFLAG(IS_WIN)
        last_slot = slot_index;
#endif
      }

      slot_span->SetFreelistHead(head);

      PA_DCHECK(num_new_entries == num_slots - slot_span->num_allocated_slots);

#if BUILDFLAG(USE_FREESLOT_BITMAP)
      FreeSlotBitmapReset(slot_span_start + (slot_size * num_slots), end_addr,
                          slot_size);
#endif

      // Discard the memory.
      ScopedSyscallTimer timer{root};
      DiscardSystemPages(begin_addr, unprovisioned_bytes);
    }
  }

  if (slot_size < SystemPageSize()) {
    // Returns here because implementing the following steps for smaller slot
    // size will need a complicated logic and make the code messy.
    return discardable_bytes;
  }

  // Next, walk the slots and for any not in use, consider which system pages
  // are no longer needed. We can release any system pages back to the system as
  // long as we don't interfere with a freelist pointer or an adjacent used
  // slot.
  for (size_t i = 0; i < num_slots; ++i) {
    if (slot_usage[i]) {
      continue;
    }

    // The first address we can safely discard is just after the freelist
    // pointer. There's one quirk: if the freelist pointer is actually nullptr,
    // we can discard that pointer value too.
    uintptr_t begin_addr = slot_span_start + (i * slot_size);
    uintptr_t end_addr = begin_addr + slot_size;

    bool can_discard_free_list_pointer = false;
#if !BUILDFLAG(IS_WIN)
    if (i != last_slot) {
      begin_addr += sizeof(internal::PartitionFreelistEntry);
    } else {
      can_discard_free_list_pointer = true;
    }
#else
    begin_addr += sizeof(internal::PartitionFreelistEntry);
#endif

    uintptr_t rounded_up_begin_addr = RoundUpToSystemPage(begin_addr);
    uintptr_t rounded_down_begin_addr = RoundDownToSystemPage(begin_addr);
    end_addr = RoundDownToSystemPage(end_addr);

    // |rounded_up_begin_addr| could be greater than |end_addr| only if slot
    // size was less than system page size, or if free list pointer crossed the
    // page boundary. Neither is possible here.
    PA_DCHECK(rounded_up_begin_addr <= end_addr);

    if (rounded_down_begin_addr < rounded_up_begin_addr && i != 0 &&
        !slot_usage[i - 1] && can_discard_free_list_pointer) {
      // This slot contains a partial page in the beginning. The rest of that
      // page is contained in the slot[i-1], which is also discardable.
      // Therefore we can discard this page.
      begin_addr = rounded_down_begin_addr;
    } else {
      begin_addr = rounded_up_begin_addr;
    }

    if (begin_addr < end_addr) {
      size_t partial_slot_bytes = end_addr - begin_addr;
      discardable_bytes += partial_slot_bytes;
      if (discard) {
        ScopedSyscallTimer timer{root};
        DiscardSystemPages(begin_addr, partial_slot_bytes);
      }
    }
  }

  return discardable_bytes;
}

template <bool thread_safe>
static void PartitionPurgeBucket(
    internal::PartitionBucket<thread_safe>* bucket) {
  if (bucket->active_slot_spans_head !=
      internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
    for (internal::SlotSpanMetadata<thread_safe>* slot_span =
             bucket->active_slot_spans_head;
         slot_span; slot_span = slot_span->next_slot_span) {
      PA_DCHECK(
          slot_span !=
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
      PartitionPurgeSlotSpan(slot_span, true);
    }
  }
}

template <bool thread_safe>
static void PartitionDumpSlotSpanStats(
    PartitionBucketMemoryStats* stats_out,
    internal::SlotSpanMetadata<thread_safe>* slot_span) {
  uint16_t bucket_num_slots = slot_span->bucket->get_slots_per_span();

  if (slot_span->is_decommitted()) {
    ++stats_out->num_decommitted_slot_spans;
    return;
  }

  stats_out->discardable_bytes += PartitionPurgeSlotSpan(slot_span, false);

  if (slot_span->CanStoreRawSize()) {
    stats_out->active_bytes += static_cast<uint32_t>(slot_span->GetRawSize());
  } else {
    stats_out->active_bytes +=
        (slot_span->num_allocated_slots * stats_out->bucket_slot_size);
  }
  stats_out->active_count += slot_span->num_allocated_slots;

  size_t slot_span_bytes_resident = RoundUpToSystemPage(
      (bucket_num_slots - slot_span->num_unprovisioned_slots) *
      stats_out->bucket_slot_size);
  stats_out->resident_bytes += slot_span_bytes_resident;
  if (slot_span->is_empty()) {
    stats_out->decommittable_bytes += slot_span_bytes_resident;
    ++stats_out->num_empty_slot_spans;
  } else if (slot_span->is_full()) {
    ++stats_out->num_full_slot_spans;
  } else {
    PA_DCHECK(slot_span->is_active());
    ++stats_out->num_active_slot_spans;
  }
}

template <bool thread_safe>
static void PartitionDumpBucketStats(
    PartitionBucketMemoryStats* stats_out,
    const internal::PartitionBucket<thread_safe>* bucket) {
  PA_DCHECK(!bucket->is_direct_mapped());
  stats_out->is_valid = false;
  // If the active slot span list is empty (==
  // internal::SlotSpanMetadata::get_sentinel_slot_span()), the bucket might
  // still need to be reported if it has a list of empty, decommitted or full
  // slot spans.
  if (bucket->active_slot_spans_head ==
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span() &&
      !bucket->empty_slot_spans_head && !bucket->decommitted_slot_spans_head &&
      !bucket->num_full_slot_spans) {
    return;
  }

  memset(stats_out, '\0', sizeof(*stats_out));
  stats_out->is_valid = true;
  stats_out->is_direct_map = false;
  stats_out->num_full_slot_spans =
      static_cast<size_t>(bucket->num_full_slot_spans);
  stats_out->bucket_slot_size = bucket->slot_size;
  uint16_t bucket_num_slots = bucket->get_slots_per_span();
  size_t bucket_useful_storage = stats_out->bucket_slot_size * bucket_num_slots;
  stats_out->allocated_slot_span_size = bucket->get_bytes_per_span();
  stats_out->active_bytes = bucket->num_full_slot_spans * bucket_useful_storage;
  stats_out->active_count = bucket->num_full_slot_spans * bucket_num_slots;
  stats_out->resident_bytes =
      bucket->num_full_slot_spans * stats_out->allocated_slot_span_size;

  for (internal::SlotSpanMetadata<thread_safe>* slot_span =
           bucket->empty_slot_spans_head;
       slot_span; slot_span = slot_span->next_slot_span) {
    PA_DCHECK(slot_span->is_empty() || slot_span->is_decommitted());
    PartitionDumpSlotSpanStats(stats_out, slot_span);
  }
  for (internal::SlotSpanMetadata<thread_safe>* slot_span =
           bucket->decommitted_slot_spans_head;
       slot_span; slot_span = slot_span->next_slot_span) {
    PA_DCHECK(slot_span->is_decommitted());
    PartitionDumpSlotSpanStats(stats_out, slot_span);
  }

  if (bucket->active_slot_spans_head !=
      internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
    for (internal::SlotSpanMetadata<thread_safe>* slot_span =
             bucket->active_slot_spans_head;
         slot_span; slot_span = slot_span->next_slot_span) {
      PA_DCHECK(
          slot_span !=
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
      PartitionDumpSlotSpanStats(stats_out, slot_span);
    }
  }
}

#if BUILDFLAG(PA_DCHECK_IS_ON)
void DCheckIfManagedByPartitionAllocBRPPool(uintptr_t address) {
  PA_DCHECK(IsManagedByPartitionAllocBRPPool(address));
}
#endif

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAllocThreadIsolationInit(ThreadIsolationOption thread_isolation) {
#if BUILDFLAG(PA_DCHECK_IS_ON)
  ThreadIsolationSettings::settings.enabled = true;
#endif
  PartitionAddressSpace::InitThreadIsolatedPool(thread_isolation);
  // Call WriteProtectThreadIsolatedGlobals last since we might not have write
  // permissions to to globals afterwards.
  WriteProtectThreadIsolatedGlobals(thread_isolation);
}
#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)

}  // namespace internal

template <bool thread_safe>
[[noreturn]] PA_NOINLINE void PartitionRoot<thread_safe>::OutOfMemory(
    size_t size) {
  const size_t virtual_address_space_size =
      total_size_of_super_pages.load(std::memory_order_relaxed) +
      total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);
#if !defined(ARCH_CPU_64_BITS)
  const size_t uncommitted_size =
      virtual_address_space_size -
      total_size_of_committed_pages.load(std::memory_order_relaxed);

  // Check whether this OOM is due to a lot of super pages that are allocated
  // but not committed, probably due to http://crbug.com/421387.
  if (uncommitted_size > internal::kReasonableSizeOfUnusedPages) {
    internal::PartitionOutOfMemoryWithLotsOfUncommitedPages(size);
  }

#if BUILDFLAG(IS_WIN)
  // If true then we are running on 64-bit Windows.
  BOOL is_wow_64 = FALSE;
  // Intentionally ignoring failures.
  IsWow64Process(GetCurrentProcess(), &is_wow_64);
  // 32-bit address space on Windows is typically either 2 GiB (on 32-bit
  // Windows) or 4 GiB (on 64-bit Windows). 2.8 and 1.0 GiB are just rough
  // guesses as to how much address space PA can consume (note that code,
  // stacks, and other allocators will also consume address space).
  const size_t kReasonableVirtualSize = (is_wow_64 ? 2800 : 1024) * 1024 * 1024;
  // Make it obvious whether we are running on 64-bit Windows.
  PA_DEBUG_DATA_ON_STACK("is_wow_64", static_cast<size_t>(is_wow_64));
#else
  constexpr size_t kReasonableVirtualSize =
      // 1.5GiB elsewhere, since address space is typically 3GiB.
      (1024 + 512) * 1024 * 1024;
#endif
  if (virtual_address_space_size > kReasonableVirtualSize) {
    internal::PartitionOutOfMemoryWithLargeVirtualSize(
        virtual_address_space_size);
  }
#endif  // #if !defined(ARCH_CPU_64_BITS)

  // Out of memory can be due to multiple causes, such as:
  // - Out of virtual address space in the desired pool
  // - Out of commit due to either our process, or another one
  // - Excessive allocations in the current process
  //
  // Saving these values make it easier to distinguish between these. See the
  // documentation in PA_CONFIG(DEBUG_DATA_ON_STACK) on how to get these from
  // minidumps.
  PA_DEBUG_DATA_ON_STACK("va_size", virtual_address_space_size);
  PA_DEBUG_DATA_ON_STACK("alloc", get_total_size_of_allocated_bytes());
  PA_DEBUG_DATA_ON_STACK("commit", get_total_size_of_committed_pages());
  PA_DEBUG_DATA_ON_STACK("size", size);

  if (internal::g_oom_handling_function) {
    (*internal::g_oom_handling_function)(size);
  }
  OOM_CRASH(size);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DecommitEmptySlotSpans() {
  ShrinkEmptySlotSpansRing(0);
  // Just decommitted everything, and holding the lock, should be exactly 0.
  PA_DCHECK(empty_slot_spans_dirty_bytes == 0);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DestructForTesting() {
  // We need to destruct the thread cache before we unreserve any of the super
  // pages below, which we currently are not doing. So, we should only call
  // this function on PartitionRoots without a thread cache.
  PA_CHECK(!flags.with_thread_cache);
  auto pool_handle = ChoosePool();
#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // The pages managed by thread isolated pool will be free-ed at
  // UninitThreadIsolatedForTesting(). Don't invoke FreePages() for the pages.
  if (pool_handle == internal::kThreadIsolatedPoolHandle) {
    return;
  }
  PA_DCHECK(pool_handle < internal::kNumPools);
#else
  PA_DCHECK(pool_handle <= internal::kNumPools);
#endif

  auto* curr = first_extent;
  while (curr != nullptr) {
    auto* next = curr->next;
    uintptr_t address = SuperPagesBeginFromExtent(curr);
    size_t size =
        internal::kSuperPageSize * curr->number_of_consecutive_super_pages;
#if !BUILDFLAG(HAS_64_BIT_POINTERS)
    internal::AddressPoolManager::GetInstance().MarkUnused(pool_handle, address,
                                                           size);
#endif
    internal::AddressPoolManager::GetInstance().UnreserveAndDecommit(
        pool_handle, address, size);
    curr = next;
  }
}

#if PA_CONFIG(ENABLE_MAC11_MALLOC_SIZE_HACK)
template <bool thread_safe>
void PartitionRoot<thread_safe>::InitMac11MallocSizeHackUsableSize(
    size_t ref_count_size) {
  flags.mac11_malloc_size_hack_enabled_ = true;

  // 0 means reserve just enough extras to fit PartitionRefCount.
  if (!ref_count_size) {
    ref_count_size = sizeof(internal::PartitionRefCount);
  }
  // Request of 32B will fall into a 48B bucket in the presence of BRP
  // ref-count, yielding |48 - ref_count_size| of actual usable space.
  flags.mac11_malloc_size_hack_usable_size_ = 48 - ref_count_size;
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::EnableMac11MallocSizeHackForTesting(
    size_t ref_count_size) {
  flags.mac11_malloc_size_hack_enabled_ = true;
  InitMac11MallocSizeHackUsableSize(ref_count_size);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::EnableMac11MallocSizeHackIfNeeded(
    size_t ref_count_size) {
  flags.mac11_malloc_size_hack_enabled_ =
      flags.brp_enabled_ && internal::base::mac::IsOS11();
  if (flags.mac11_malloc_size_hack_enabled_) {
    InitMac11MallocSizeHackUsableSize(ref_count_size);
  }
}
#endif  // PA_CONFIG(ENABLE_MAC11_MALLOC_SIZE_HACK)

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && !BUILDFLAG(HAS_64_BIT_POINTERS)
namespace {
std::atomic<bool> g_reserve_brp_guard_region_called;
// An address constructed by repeating `kQuarantinedByte` shouldn't never point
// to valid memory. Preemptively reserve a memory region around that address and
// make it inaccessible. Not needed for 64-bit platforms where the address is
// guaranteed to be non-canonical. Safe to call multiple times.
void ReserveBackupRefPtrGuardRegionIfNeeded() {
  bool expected = false;
  // No need to block execution for potential concurrent initialization, merely
  // want to make sure this is only called once.
  if (!g_reserve_brp_guard_region_called.compare_exchange_strong(expected,
                                                                 true)) {
    return;
  }

  size_t alignment = internal::PageAllocationGranularity();
  uintptr_t requested_address;
  memset(&requested_address, internal::kQuarantinedByte,
         sizeof(requested_address));
  requested_address = RoundDownToPageAllocationGranularity(requested_address);

  // Request several pages so that even unreasonably large C++ objects stay
  // within the inaccessible region. If some of the pages can't be reserved,
  // it's still preferable to try and reserve the rest.
  for (size_t i = 0; i < 4; ++i) {
    [[maybe_unused]] uintptr_t allocated_address =
        AllocPages(requested_address, alignment, alignment,
                   PageAccessibilityConfiguration(
                       PageAccessibilityConfiguration::kInaccessible),
                   PageTag::kPartitionAlloc);
    requested_address += alignment;
  }
}
}  // namespace
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) &&
        // !BUILDFLAG(HAS_64_BIT_POINTERS)

template <bool thread_safe>
void PartitionRoot<thread_safe>::Init(PartitionOptions opts) {
  {
#if BUILDFLAG(IS_APPLE)
    // Needed to statically bound page size, which is a runtime constant on
    // apple OSes.
    PA_CHECK((internal::SystemPageSize() == (size_t{1} << 12)) ||
             (internal::SystemPageSize() == (size_t{1} << 14)));
#elif BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)
    // Check runtime pagesize. Though the code is currently the same, it is
    // not merged with the IS_APPLE case above as a 1 << 16 case needs to be
    // added here in the future, to allow 64 kiB pagesize. That is only
    // supported on Linux on arm64, not on IS_APPLE, but not yet present here
    // as the rest of the partition allocator does not currently support it.
    PA_CHECK((internal::SystemPageSize() == (size_t{1} << 12)) ||
             (internal::SystemPageSize() == (size_t{1} << 14)));
#endif

    ::partition_alloc::internal::ScopedGuard guard{lock_};
    if (initialized) {
      return;
    }

#if BUILDFLAG(HAS_64_BIT_POINTERS)
    // Reserve address space for partition alloc.
    internal::PartitionAddressSpace::Init();
#endif

#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && !BUILDFLAG(HAS_64_BIT_POINTERS)
    ReserveBackupRefPtrGuardRegionIfNeeded();
#endif

    flags.allow_aligned_alloc =
        opts.aligned_alloc == PartitionOptions::AlignedAlloc::kAllowed;
    flags.allow_cookie = opts.cookie == PartitionOptions::Cookie::kAllowed;
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    flags.brp_enabled_ =
        opts.backup_ref_ptr == PartitionOptions::BackupRefPtr::kEnabled;
#if PA_CONFIG(ENABLE_MAC11_MALLOC_SIZE_HACK)
    EnableMac11MallocSizeHackIfNeeded(opts.ref_count_size);
#endif
#else   // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_CHECK(opts.backup_ref_ptr == PartitionOptions::BackupRefPtr::kDisabled);
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    flags.use_configurable_pool =
        (opts.use_configurable_pool ==
         PartitionOptions::UseConfigurablePool::kIfAvailable) &&
        IsConfigurablePoolAvailable();
    PA_DCHECK(!flags.use_configurable_pool || IsConfigurablePoolAvailable());
#if PA_CONFIG(HAS_MEMORY_TAGGING)
    flags.memory_tagging_enabled_ =
        opts.memory_tagging == PartitionOptions::MemoryTagging::kEnabled;
    // Memory tagging is not supported in the configurable pool because MTE
    // stores tagging information in the high bits of the pointer, it causes
    // issues with components like V8's ArrayBuffers which use custom pointer
    // representations. All custom representations encountered so far rely on an
    // "is in configurable pool?" check, so we use that as a proxy.
    PA_CHECK(!flags.memory_tagging_enabled_ || !flags.use_configurable_pool);
#endif

    // brp_enabled() is not supported in the configurable pool because
    // BRP requires objects to be in a different Pool.
    PA_CHECK(!(flags.use_configurable_pool && brp_enabled()));

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
    // BRP and thread isolated mode use different pools, so they can't be
    // enabled at the same time.
    PA_CHECK(!opts.thread_isolation.enabled ||
             opts.backup_ref_ptr == PartitionOptions::BackupRefPtr::kDisabled);
    flags.thread_isolation = opts.thread_isolation;
#endif

    // Ref-count messes up alignment needed for AlignedAlloc, making this
    // option incompatible. However, except in the
    // PUT_REF_COUNT_IN_PREVIOUS_SLOT case.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && \
    !BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
    PA_CHECK(!flags.allow_aligned_alloc || !flags.brp_enabled_);
#endif

#if PA_CONFIG(EXTRAS_REQUIRED)
    flags.extras_size = 0;
    flags.extras_offset = 0;

    if (flags.allow_cookie) {
      flags.extras_size += internal::kPartitionCookieSizeAdjustment;
    }

    if (brp_enabled()) {
      // TODO(tasak): In the PUT_REF_COUNT_IN_PREVIOUS_SLOT case, ref-count is
      // stored out-of-line for single-slot slot spans, so no need to
      // add/subtract its size in this case.
      size_t ref_count_size = opts.ref_count_size;
      if (!ref_count_size) {
        ref_count_size = internal::kPartitionRefCountSizeAdjustment;
      }
#if PA_CONFIG(INCREASE_REF_COUNT_SIZE_FOR_MTE)
      if (IsMemoryTaggingEnabled()) {
        ref_count_size = internal::base::bits::AlignUp(
            ref_count_size, internal::kMemTagGranuleSize);
      }
      flags.ref_count_size = ref_count_size;
#endif  // PA_CONFIG(INCREASE_REF_COUNT_SIZE_FOR_MTE)
      PA_CHECK(internal::kPartitionRefCountSizeAdjustment <= ref_count_size);
      flags.extras_size += ref_count_size;
      flags.extras_offset += internal::kPartitionRefCountOffsetAdjustment;
    }
#endif  // PA_CONFIG(EXTRAS_REQUIRED)

    // Re-confirm the above PA_CHECKs, by making sure there are no
    // pre-allocation extras when AlignedAlloc is allowed. Post-allocation
    // extras are ok.
    PA_CHECK(!flags.allow_aligned_alloc || !flags.extras_offset);

    flags.quarantine_mode =
#if BUILDFLAG(USE_STARSCAN)
        (opts.quarantine == PartitionOptions::Quarantine::kDisallowed
             ? QuarantineMode::kAlwaysDisabled
             : QuarantineMode::kDisabledByDefault);
#else
        QuarantineMode::kAlwaysDisabled;
#endif  // BUILDFLAG(USE_STARSCAN)

    // We mark the sentinel slot span as free to make sure it is skipped by our
    // logic to find a new active slot span.
    memset(&sentinel_bucket, 0, sizeof(sentinel_bucket));
    sentinel_bucket.active_slot_spans_head =
        SlotSpan::get_sentinel_slot_span_non_const();

    // This is a "magic" value so we can test if a root pointer is valid.
    inverted_self = ~reinterpret_cast<uintptr_t>(this);

    // Set up the actual usable buckets first.
    constexpr internal::BucketIndexLookup lookup{};
    size_t bucket_index = 0;
    while (lookup.bucket_sizes()[bucket_index] !=
           internal::kInvalidBucketSize) {
      buckets[bucket_index].Init(lookup.bucket_sizes()[bucket_index]);
      bucket_index++;
    }
    PA_DCHECK(bucket_index < internal::kNumBuckets);

    // Remaining buckets are not usable, and not real.
    for (size_t index = bucket_index; index < internal::kNumBuckets; index++) {
      // Cannot init with size 0 since it computes 1 / size, but make sure the
      // bucket is invalid.
      buckets[index].Init(internal::kInvalidBucketSize);
      buckets[index].active_slot_spans_head = nullptr;
      PA_DCHECK(!buckets[index].is_valid());
    }

#if !PA_CONFIG(THREAD_CACHE_SUPPORTED)
    // TLS in ThreadCache not supported on other OSes.
    flags.with_thread_cache = false;
#else
    ThreadCache::EnsureThreadSpecificDataInitialized();
    flags.with_thread_cache =
        (opts.thread_cache == PartitionOptions::ThreadCache::kEnabled);

    if (flags.with_thread_cache) {
      ThreadCache::Init(this);
    }
#endif  // !PA_CONFIG(THREAD_CACHE_SUPPORTED)

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)
    internal::PartitionRootEnumerator::Instance().Register(this);
#endif

    initialized = true;
  }

  // Called without the lock, might allocate.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PartitionAllocMallocInitOnce();
#endif

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)
  if (flags.thread_isolation.enabled) {
    internal::PartitionAllocThreadIsolationInit(flags.thread_isolation);
  }
#endif
}

template <bool thread_safe>
PartitionRoot<thread_safe>::~PartitionRoot() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(!flags.with_thread_cache)
      << "Must not destroy a partition with a thread cache";
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)
  if (initialized) {
    internal::PartitionRootEnumerator::Instance().Unregister(this);
  }
#endif  // PA_CONFIG(USE_PARTITION_ALLOC_ENUMERATOR)
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::EnableThreadCacheIfSupported() {
#if PA_CONFIG(THREAD_CACHE_SUPPORTED)
  ::partition_alloc::internal::ScopedGuard guard{lock_};
  PA_CHECK(!flags.with_thread_cache);
  // By the time we get there, there may be multiple threads created in the
  // process. Since `with_thread_cache` is accessed without a lock, it can
  // become visible to another thread before the effects of
  // `internal::ThreadCacheInit()` are visible. To prevent that, we fake thread
  // cache creation being in-progress while this is running.
  //
  // This synchronizes with the acquire load in `MaybeInitThreadCacheAndAlloc()`
  // to ensure that we don't create (and thus use) a ThreadCache before
  // ThreadCache::Init()'s effects are visible.
  int before =
      thread_caches_being_constructed_.fetch_add(1, std::memory_order_acquire);
  PA_CHECK(before == 0);
  ThreadCache::Init(this);
  thread_caches_being_constructed_.fetch_sub(1, std::memory_order_release);
  flags.with_thread_cache = true;
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)
}

template <bool thread_safe>
bool PartitionRoot<thread_safe>::TryReallocInPlaceForDirectMap(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    size_t requested_size) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  // Slot-span metadata isn't MTE-tagged.
  PA_DCHECK(
      internal::IsManagedByDirectMap(reinterpret_cast<uintptr_t>(slot_span)));

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  auto* extent = DirectMapExtent::FromSlotSpan(slot_span);
  size_t current_reservation_size = extent->reservation_size;
  // Calculate the new reservation size the way PartitionDirectMap() would, but
  // skip the alignment, because this call isn't requesting it.
  size_t new_reservation_size = GetDirectMapReservationSize(raw_size);

  // If new reservation would be larger, there is nothing we can do to
  // reallocate in-place.
  if (new_reservation_size > current_reservation_size) {
    return false;
  }

  // Don't reallocate in-place if new reservation size would be less than 80 %
  // of the current one, to avoid holding on to too much unused address space.
  // Make this check before comparing slot sizes, as even with equal or similar
  // slot sizes we can save a lot if the original allocation was heavily padded
  // for alignment.
  if ((new_reservation_size >> internal::SystemPageShift()) * 5 <
      (current_reservation_size >> internal::SystemPageShift()) * 4) {
    return false;
  }

  // Note that the new size isn't a bucketed size; this function is called
  // whenever we're reallocating a direct mapped allocation, so calculate it
  // the way PartitionDirectMap() would.
  size_t new_slot_size = GetDirectMapSlotSize(raw_size);
  if (new_slot_size < internal::kMinDirectMappedDownsize) {
    return false;
  }

  // Past this point, we decided we'll attempt to reallocate without relocating,
  // so we have to honor the padding for alignment in front of the original
  // allocation, even though this function isn't requesting any alignment.

  // bucket->slot_size is the currently committed size of the allocation.
  size_t current_slot_size = slot_span->bucket->slot_size;
  size_t current_usable_size = slot_span->GetUsableSize(this);
  uintptr_t slot_start = SlotSpan::ToSlotSpanStart(slot_span);
  // This is the available part of the reservation up to which the new
  // allocation can grow.
  size_t available_reservation_size =
      current_reservation_size - extent->padding_for_alignment -
      PartitionRoot<thread_safe>::GetDirectMapMetadataAndGuardPagesSize();
#if BUILDFLAG(PA_DCHECK_IS_ON)
  uintptr_t reservation_start = slot_start & internal::kSuperPageBaseMask;
  PA_DCHECK(internal::IsReservationStart(reservation_start));
  PA_DCHECK(slot_start + available_reservation_size ==
            reservation_start + current_reservation_size -
                GetDirectMapMetadataAndGuardPagesSize() +
                internal::PartitionPageSize());
#endif

  PA_DCHECK(new_slot_size > internal::kMaxMemoryTaggingSize);
  if (new_slot_size == current_slot_size) {
    // No need to move any memory around, but update size and cookie below.
    // That's because raw_size may have changed.
  } else if (new_slot_size < current_slot_size) {
    // Shrink by decommitting unneeded pages and making them inaccessible.
    size_t decommit_size = current_slot_size - new_slot_size;
    DecommitSystemPagesForData(slot_start + new_slot_size, decommit_size,
                               PageAccessibilityDisposition::kRequireUpdate);
    // Since the decommited system pages are still reserved, we don't need to
    // change the entries for decommitted pages in the reservation offset table.
  } else if (new_slot_size <= available_reservation_size) {
    // Grow within the actually reserved address space. Just need to make the
    // pages accessible again.
    size_t recommit_slot_size_growth = new_slot_size - current_slot_size;
    // Direct map never uses tagging, as size is always >kMaxMemoryTaggingSize.
    RecommitSystemPagesForData(
        slot_start + current_slot_size, recommit_slot_size_growth,
        PageAccessibilityDisposition::kRequireUpdate, false);
    // The recommited system pages had been already reserved and all the
    // entries in the reservation offset table (for entire reservation_size
    // region) have been already initialized.

#if BUILDFLAG(PA_DCHECK_IS_ON)
    memset(reinterpret_cast<void*>(slot_start + current_slot_size),
           internal::kUninitializedByte, recommit_slot_size_growth);
#endif
  } else {
    // We can't perform the realloc in-place.
    // TODO: support this too when possible.
    return false;
  }

  DecreaseTotalSizeOfAllocatedBytes(reinterpret_cast<uintptr_t>(slot_span),
                                    slot_span->bucket->slot_size);
  slot_span->SetRawSize(raw_size);
  slot_span->bucket->slot_size = new_slot_size;
  IncreaseTotalSizeOfAllocatedBytes(reinterpret_cast<uintptr_t>(slot_span),
                                    slot_span->bucket->slot_size, raw_size);

  // Always record in-place realloc() as free()+malloc() pair.
  //
  // The early returns above (`return false`) will fall back to free()+malloc(),
  // so this is consistent.
  auto* thread_cache = GetOrCreateThreadCache();
  if (ThreadCache::IsValid(thread_cache)) {
    thread_cache->RecordDeallocation(current_usable_size);
    thread_cache->RecordAllocation(slot_span->GetUsableSize(this));
  }

#if BUILDFLAG(PA_DCHECK_IS_ON)
  // Write a new trailing cookie.
  if (flags.allow_cookie) {
    auto* object = static_cast<unsigned char*>(SlotStartToObject(slot_start));
    internal::PartitionCookieWriteValue(object +
                                        slot_span->GetUsableSize(this));
  }
#endif

  return true;
}

template <bool thread_safe>
bool PartitionRoot<thread_safe>::TryReallocInPlaceForNormalBuckets(
    void* object,
    SlotSpan* slot_span,
    size_t new_size) {
  uintptr_t slot_start = ObjectToSlotStart(object);
  PA_DCHECK(internal::IsManagedByNormalBuckets(slot_start));

  // TODO: note that tcmalloc will "ignore" a downsizing realloc() unless the
  // new size is a significant percentage smaller. We could do the same if we
  // determine it is a win.
  if (AllocationCapacityFromRequestedSize(new_size) !=
      AllocationCapacityFromSlotStart(slot_start)) {
    return false;
  }
  size_t current_usable_size = slot_span->GetUsableSize(this);

  // Trying to allocate |new_size| would use the same amount of underlying
  // memory as we're already using, so re-use the allocation after updating
  // statistics (and cookie, if present).
  if (slot_span->CanStoreRawSize()) {
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && BUILDFLAG(PA_DCHECK_IS_ON)
    internal::PartitionRefCount* old_ref_count;
    if (brp_enabled()) {
      old_ref_count = internal::PartitionRefCountPointer(slot_start);
    }
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) &&
        // BUILDFLAG(PA_DCHECK_IS_ON)
    size_t new_raw_size = AdjustSizeForExtrasAdd(new_size);
    slot_span->SetRawSize(new_raw_size);
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) && BUILDFLAG(PA_DCHECK_IS_ON)
    if (brp_enabled()) {
      internal::PartitionRefCount* new_ref_count =
          internal::PartitionRefCountPointer(slot_start);
      PA_DCHECK(new_ref_count == old_ref_count);
    }
#endif  // BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) &&
        // BUILDFLAG(PA_DCHECK_IS_ON)
#if BUILDFLAG(PA_DCHECK_IS_ON)
    // Write a new trailing cookie only when it is possible to keep track
    // raw size (otherwise we wouldn't know where to look for it later).
    if (flags.allow_cookie) {
      internal::PartitionCookieWriteValue(static_cast<unsigned char*>(object) +
                                          slot_span->GetUsableSize(this));
    }
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)
  }

  // Always record a realloc() as a free() + malloc(), even if it's in
  // place. When we cannot do it in place (`return false` above), the allocator
  // falls back to free()+malloc(), so this is consistent.
  ThreadCache* thread_cache = GetOrCreateThreadCache();
  if (PA_LIKELY(ThreadCache::IsValid(thread_cache))) {
    thread_cache->RecordDeallocation(current_usable_size);
    thread_cache->RecordAllocation(slot_span->GetUsableSize(this));
  }

  return object;
}

template <bool thread_safe>
void* PartitionRoot<thread_safe>::ReallocWithFlags(unsigned int flags,
                                                   void* ptr,
                                                   size_t new_size,
                                                   const char* type_name) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  CHECK_MAX_SIZE_OR_RETURN_NULLPTR(new_size, flags);
  void* result = realloc(ptr, new_size);
  PA_CHECK(result || flags & AllocFlags::kReturnNull);
  return result;
#else
  bool no_hooks = flags & AllocFlags::kNoHooks;
  if (PA_UNLIKELY(!ptr)) {
    return no_hooks
               ? AllocWithFlagsNoHooks(flags, new_size,
                                       internal::PartitionPageSize())
               : AllocWithFlagsInternal(
                     flags, new_size, internal::PartitionPageSize(), type_name);
  }

  if (PA_UNLIKELY(!new_size)) {
    Free(ptr);
    return nullptr;
  }

  if (new_size > internal::MaxDirectMapped()) {
    if (flags & AllocFlags::kReturnNull) {
      return nullptr;
    }
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  bool overridden = false;
  size_t old_usable_size;
  if (PA_UNLIKELY(!no_hooks && hooks_enabled)) {
    overridden = PartitionAllocHooks::ReallocOverrideHookIfEnabled(
        &old_usable_size, ptr);
  }
  if (PA_LIKELY(!overridden)) {
    // |ptr| may have been allocated in another root.
    SlotSpan* slot_span = SlotSpan::FromObject(ptr);
    auto* old_root = PartitionRoot::FromSlotSpan(slot_span);
    bool success = false;
    bool tried_in_place_for_direct_map = false;
    {
      ::partition_alloc::internal::ScopedGuard guard{old_root->lock_};
      // TODO(crbug.com/1257655): See if we can afford to make this a CHECK.
      PA_DCHECK(IsValidSlotSpan(slot_span));
      old_usable_size = slot_span->GetUsableSize(old_root);

      if (PA_UNLIKELY(slot_span->bucket->is_direct_mapped())) {
        tried_in_place_for_direct_map = true;
        // We may be able to perform the realloc in place by changing the
        // accessibility of memory pages and, if reducing the size, decommitting
        // them.
        success = old_root->TryReallocInPlaceForDirectMap(slot_span, new_size);
      }
    }
    if (success) {
      if (PA_UNLIKELY(!no_hooks && hooks_enabled)) {
        PartitionAllocHooks::ReallocObserverHookIfEnabled(ptr, ptr, new_size,
                                                          type_name);
      }
      return ptr;
    }

    if (PA_LIKELY(!tried_in_place_for_direct_map)) {
      if (old_root->TryReallocInPlaceForNormalBuckets(ptr, slot_span,
                                                      new_size)) {
        return ptr;
      }
    }
  }

  // This realloc cannot be resized in-place. Sadness.
  void* ret =
      no_hooks ? AllocWithFlagsNoHooks(flags, new_size,
                                       internal::PartitionPageSize())
               : AllocWithFlagsInternal(
                     flags, new_size, internal::PartitionPageSize(), type_name);
  if (!ret) {
    if (flags & AllocFlags::kReturnNull) {
      return nullptr;
    }
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  memcpy(ret, ptr, std::min(old_usable_size, new_size));
  Free(ptr);  // Implicitly protects the old ptr on MTE systems.
  return ret;
#endif
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::PurgeMemory(int flags) {
  {
    ::partition_alloc::internal::ScopedGuard guard{lock_};
#if BUILDFLAG(USE_STARSCAN)
    // Avoid purging if there is PCScan task currently scheduled. Since pcscan
    // takes snapshot of all allocated pages, decommitting pages here (even
    // under the lock) is racy.
    // TODO(bikineev): Consider rescheduling the purging after PCScan.
    if (PCScan::IsInProgress()) {
      return;
    }
#endif  // BUILDFLAG(USE_STARSCAN)

    if (flags & PurgeFlags::kDecommitEmptySlotSpans) {
      DecommitEmptySlotSpans();
    }
    if (flags & PurgeFlags::kDiscardUnusedSystemPages) {
      for (Bucket& bucket : buckets) {
        if (bucket.slot_size == internal::kInvalidBucketSize) {
          continue;
        }

        if (bucket.slot_size >= internal::MinPurgeableSlotSize()) {
          internal::PartitionPurgeBucket(&bucket);
        } else {
          bucket.SortSlotSpanFreelists();
        }

        // Do it at the end, as the actions above change the status of slot
        // spans (e.g. empty -> decommitted).
        bucket.MaintainActiveList();

        if (sort_active_slot_spans_) {
          bucket.SortActiveSlotSpans();
        }
      }
    }
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::ShrinkEmptySlotSpansRing(size_t limit) {
  int16_t index = global_empty_slot_span_ring_index;
  int16_t starting_index = index;
  while (empty_slot_spans_dirty_bytes > limit) {
    SlotSpan* slot_span = global_empty_slot_span_ring[index];
    // The ring is not always full, may be nullptr.
    if (slot_span) {
      slot_span->DecommitIfPossible(this);
      global_empty_slot_span_ring[index] = nullptr;
    }
    index += 1;
    // Walk through the entirety of possible slots, even though the last ones
    // are unused, if global_empty_slot_span_ring_size is smaller than
    // kMaxFreeableSpans. It's simpler, and does not cost anything, since all
    // the pointers are going to be nullptr.
    if (index == internal::kMaxFreeableSpans) {
      index = 0;
    }

    // Went around the whole ring, since this is locked,
    // empty_slot_spans_dirty_bytes should be exactly 0.
    if (index == starting_index) {
      PA_DCHECK(empty_slot_spans_dirty_bytes == 0);
      // Metrics issue, don't crash, return.
      break;
    }
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::DumpStats(const char* partition_name,
                                           bool is_light_dump,
                                           PartitionStatsDumper* dumper) {
  static const size_t kMaxReportableDirectMaps = 4096;
  // Allocate on the heap rather than on the stack to avoid stack overflow
  // skirmishes (on Windows, in particular). Allocate before locking below,
  // otherwise when PartitionAlloc is malloc() we get reentrancy issues. This
  // inflates reported values a bit for detailed dumps though, by 16kiB.
  std::unique_ptr<uint32_t[]> direct_map_lengths;
  if (!is_light_dump) {
    direct_map_lengths =
        std::unique_ptr<uint32_t[]>(new uint32_t[kMaxReportableDirectMaps]);
  }
  PartitionBucketMemoryStats bucket_stats[internal::kNumBuckets];
  size_t num_direct_mapped_allocations = 0;
  PartitionMemoryStats stats = {0};

  stats.syscall_count = syscall_count.load(std::memory_order_relaxed);
  stats.syscall_total_time_ns =
      syscall_total_time_ns.load(std::memory_order_relaxed);

  // Collect data with the lock held, cannot allocate or call third-party code
  // below.
  {
    ::partition_alloc::internal::ScopedGuard guard{lock_};
    PA_DCHECK(total_size_of_allocated_bytes <= max_size_of_allocated_bytes);

    stats.total_mmapped_bytes =
        total_size_of_super_pages.load(std::memory_order_relaxed) +
        total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);
    stats.total_committed_bytes =
        total_size_of_committed_pages.load(std::memory_order_relaxed);
    stats.max_committed_bytes =
        max_size_of_committed_pages.load(std::memory_order_relaxed);
    stats.total_allocated_bytes = total_size_of_allocated_bytes;
    stats.max_allocated_bytes = max_size_of_allocated_bytes;
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    stats.total_brp_quarantined_bytes =
        total_size_of_brp_quarantined_bytes.load(std::memory_order_relaxed);
    stats.total_brp_quarantined_count =
        total_count_of_brp_quarantined_slots.load(std::memory_order_relaxed);
    stats.cumulative_brp_quarantined_bytes =
        cumulative_size_of_brp_quarantined_bytes.load(
            std::memory_order_relaxed);
    stats.cumulative_brp_quarantined_count =
        cumulative_count_of_brp_quarantined_slots.load(
            std::memory_order_relaxed);
#endif

    size_t direct_mapped_allocations_total_size = 0;
    for (size_t i = 0; i < internal::kNumBuckets; ++i) {
      const Bucket* bucket = &bucket_at(i);
      // Don't report the pseudo buckets that the generic allocator sets up in
      // order to preserve a fast size->bucket map (see
      // PartitionRoot::Init() for details).
      if (!bucket->is_valid()) {
        bucket_stats[i].is_valid = false;
      } else {
        internal::PartitionDumpBucketStats(&bucket_stats[i], bucket);
      }
      if (bucket_stats[i].is_valid) {
        stats.total_resident_bytes += bucket_stats[i].resident_bytes;
        stats.total_active_bytes += bucket_stats[i].active_bytes;
        stats.total_active_count += bucket_stats[i].active_count;
        stats.total_decommittable_bytes += bucket_stats[i].decommittable_bytes;
        stats.total_discardable_bytes += bucket_stats[i].discardable_bytes;
      }
    }

    for (DirectMapExtent* extent = direct_map_list;
         extent && num_direct_mapped_allocations < kMaxReportableDirectMaps;
         extent = extent->next_extent, ++num_direct_mapped_allocations) {
      PA_DCHECK(!extent->next_extent ||
                extent->next_extent->prev_extent == extent);
      size_t slot_size = extent->bucket->slot_size;
      direct_mapped_allocations_total_size += slot_size;
      if (is_light_dump) {
        continue;
      }
      direct_map_lengths[num_direct_mapped_allocations] = slot_size;
    }

    stats.total_resident_bytes += direct_mapped_allocations_total_size;
    stats.total_active_bytes += direct_mapped_allocations_total_size;
    stats.total_active_count += num_direct_mapped_allocations;

    stats.has_thread_cache = flags.with_thread_cache;
    if (stats.has_thread_cache) {
      ThreadCacheRegistry::Instance().DumpStats(
          true, &stats.current_thread_cache_stats);
      ThreadCacheRegistry::Instance().DumpStats(false,
                                                &stats.all_thread_caches_stats);
    }
  }

  // Do not hold the lock when calling |dumper|, as it may allocate.
  if (!is_light_dump) {
    for (auto& stat : bucket_stats) {
      if (stat.is_valid) {
        dumper->PartitionsDumpBucketStats(partition_name, &stat);
      }
    }

    for (size_t i = 0; i < num_direct_mapped_allocations; ++i) {
      uint32_t size = direct_map_lengths[i];

      PartitionBucketMemoryStats mapped_stats = {};
      mapped_stats.is_valid = true;
      mapped_stats.is_direct_map = true;
      mapped_stats.num_full_slot_spans = 1;
      mapped_stats.allocated_slot_span_size = size;
      mapped_stats.bucket_slot_size = size;
      mapped_stats.active_bytes = size;
      mapped_stats.active_count = 1;
      mapped_stats.resident_bytes = size;
      dumper->PartitionsDumpBucketStats(partition_name, &mapped_stats);
    }
  }
  dumper->PartitionDumpTotals(partition_name, &stats);
}

// static
template <bool thread_safe>
void PartitionRoot<thread_safe>::DeleteForTesting(
    PartitionRoot* partition_root) {
  if (partition_root->flags.with_thread_cache) {
    ThreadCache::SwapForTesting(nullptr);
    partition_root->flags.with_thread_cache = false;
  }

  partition_root->DestructForTesting();  // IN-TEST

  delete partition_root;
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::ResetForTesting(bool allow_leaks) {
  if (flags.with_thread_cache) {
    ThreadCache::SwapForTesting(nullptr);
    flags.with_thread_cache = false;
  }

  ::partition_alloc::internal::ScopedGuard guard(lock_);

#if BUILDFLAG(PA_DCHECK_IS_ON)
  if (!allow_leaks) {
    unsigned num_allocated_slots = 0;
    for (Bucket& bucket : buckets) {
      if (bucket.active_slot_spans_head !=
          internal::SlotSpanMetadata<thread_safe>::get_sentinel_slot_span()) {
        for (internal::SlotSpanMetadata<thread_safe>* slot_span =
                 bucket.active_slot_spans_head;
             slot_span; slot_span = slot_span->next_slot_span) {
          num_allocated_slots += slot_span->num_allocated_slots;
        }
      }
      // Full slot spans are nowhere. Need to see bucket.num_full_slot_spans
      // to count the number of full slot spans' slots.
      if (bucket.num_full_slot_spans) {
        num_allocated_slots +=
            bucket.num_full_slot_spans * bucket.get_slots_per_span();
      }
    }
    PA_DCHECK(num_allocated_slots == 0);

    // Check for direct-mapped allocations.
    PA_DCHECK(!direct_map_list);
  }
#endif

  DestructForTesting();  // IN-TEST

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)
  if (initialized) {
    internal::PartitionRootEnumerator::Instance().Unregister(this);
  }
#endif  // PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)

  for (Bucket& bucket : buckets) {
    bucket.active_slot_spans_head =
        SlotSpan::get_sentinel_slot_span_non_const();
    bucket.empty_slot_spans_head = nullptr;
    bucket.decommitted_slot_spans_head = nullptr;
    bucket.num_full_slot_spans = 0;
  }

  next_super_page = 0;
  next_partition_page = 0;
  next_partition_page_end = 0;
  current_extent = nullptr;
  first_extent = nullptr;

  direct_map_list = nullptr;
  for (auto& entity : global_empty_slot_span_ring) {
    entity = nullptr;
  }

  global_empty_slot_span_ring_index = 0;
  global_empty_slot_span_ring_size = internal::kDefaultEmptySlotSpanRingSize;
  initialized = false;
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::ResetBookkeepingForTesting() {
  ::partition_alloc::internal::ScopedGuard guard{lock_};
  max_size_of_allocated_bytes = total_size_of_allocated_bytes;
  max_size_of_committed_pages.store(total_size_of_committed_pages);
}

template <>
ThreadCache* PartitionRoot<internal::ThreadSafe>::MaybeInitThreadCache() {
  auto* tcache = ThreadCache::Get();
  // See comment in `EnableThreadCacheIfSupport()` for why this is an acquire
  // load.
  if (ThreadCache::IsTombstone(tcache) ||
      thread_caches_being_constructed_.load(std::memory_order_acquire)) {
    // Two cases:
    // 1. Thread is being terminated, don't try to use the thread cache, and
    //    don't try to resurrect it.
    // 2. Someone, somewhere is currently allocating a thread cache. This may
    //    be us, in which case we are re-entering and should not create a thread
    //    cache. If it is not us, then this merely delays thread cache
    //    construction a bit, which is not an issue.
    return nullptr;
  }

  // There is no per-thread ThreadCache allocated here yet, and this partition
  // has a thread cache, allocate a new one.
  //
  // The thread cache allocation itself will not reenter here, as it sidesteps
  // the thread cache by using placement new and |RawAlloc()|. However,
  // internally to libc, allocations may happen to create a new TLS
  // variable. This would end up here again, which is not what we want (and
  // likely is not supported by libc).
  //
  // To avoid this sort of reentrancy, increase the count of thread caches that
  // are currently allocating a thread cache.
  //
  // Note that there is no deadlock or data inconsistency concern, since we do
  // not hold the lock, and has such haven't touched any internal data.
  int before =
      thread_caches_being_constructed_.fetch_add(1, std::memory_order_relaxed);
  PA_CHECK(before < std::numeric_limits<int>::max());
  tcache = ThreadCache::Create(this);
  thread_caches_being_constructed_.fetch_sub(1, std::memory_order_relaxed);

  return tcache;
}

template <>
void PartitionRoot<internal::ThreadSafe>::EnableSortActiveSlotSpans() {
  sort_active_slot_spans_ = true;
}

template struct PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    PartitionRoot<internal::ThreadSafe>;

static_assert(offsetof(PartitionRoot<internal::ThreadSafe>, sentinel_bucket) ==
                  offsetof(PartitionRoot<internal::ThreadSafe>, buckets) +
                      internal::kNumBuckets *
                          sizeof(PartitionRoot<internal::ThreadSafe>::Bucket),
              "sentinel_bucket must be just after the regular buckets.");

static_assert(
    offsetof(PartitionRoot<internal::ThreadSafe>, lock_) >= 64,
    "The lock should not be on the same cacheline as the read-mostly flags");

}  // namespace partition_alloc
