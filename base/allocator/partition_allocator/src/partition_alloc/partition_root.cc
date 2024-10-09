// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_root.h"

#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/freeslot_bitmap.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/oom.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_cookie.h"
#include "partition_alloc/partition_oom.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_superpage_extent_entry.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/tagging.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if PA_BUILDFLAG(IS_MAC)
#include "partition_alloc/partition_alloc_base/mac/mac_util.h"
#endif

#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/address_pool_manager_bitmap.h"
#endif

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>

#include "wow64apiset.h"
#endif

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
#include <sys/mman.h>
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)

namespace partition_alloc::internal {

#if PA_BUILDFLAG(RECORD_ALLOC_INFO)
// Even if this is not hidden behind a PA_BUILDFLAG, it should not use any
// memory when recording is disabled, since it ends up in the .bss section.
AllocInfo g_allocs = {};

void RecordAllocOrFree(uintptr_t addr, size_t size) {
  g_allocs.allocs[g_allocs.index.fetch_add(1, std::memory_order_relaxed) %
                  kAllocInfoSize] = {addr, size};
}
#endif  // PA_BUILDFLAG(RECORD_ALLOC_INFO)

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
PtrPosWithinAlloc IsPtrWithinSameAlloc(uintptr_t orig_address,
                                       uintptr_t test_address,
                                       size_t type_size) {
  PA_DCHECK(IsManagedByNormalBucketsOrDirectMap(orig_address));
  DCheckIfManagedByPartitionAllocBRPPool(orig_address);

  auto [slot_start, _] =
      PartitionAllocGetSlotStartAndSizeInBRPPool(orig_address);
  // Don't use |orig_address| beyond this point at all. It was needed to
  // pick the right slot, but now we're dealing with very concrete addresses.
  // Zero it just in case, to catch errors.
  orig_address = 0;

  auto* slot_span = internal::SlotSpanMetadata<
      internal::MetadataKind::kReadOnly>::FromSlotStart(slot_start);
  auto* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  // Double check that in-slot metadata is indeed present. Currently that's the
  // case only when BRP is used.
  PA_DCHECK(root->brp_enabled());

  uintptr_t object_addr = root->SlotStartToObjectAddr(slot_start);
  uintptr_t object_end = object_addr + root->GetSlotUsableSize(slot_span);
  if (test_address < object_addr || object_end < test_address) {
    return PtrPosWithinAlloc::kFarOOB;
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  } else if (object_end - type_size < test_address) {
    // Not even a single element of the type referenced by the pointer can fit
    // between the pointer and the end of the object.
    return PtrPosWithinAlloc::kAllocEnd;
#endif
  } else {
    return PtrPosWithinAlloc::kInBounds;
  }
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

}  // namespace partition_alloc::internal

namespace partition_alloc {

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)

namespace {
internal::Lock g_root_enumerator_lock;
}

internal::Lock& PartitionRoot::GetEnumeratorLock() {
  return g_root_enumerator_lock;
}

namespace internal {

class PartitionRootEnumerator {
 public:
  template <typename T>
  using EnumerateCallback = void (*)(PartitionRoot* root, T param);
  enum EnumerateOrder {
    kNormal,
    kReverse,
  };

  static PartitionRootEnumerator& Instance() {
    static PartitionRootEnumerator instance;
    return instance;
  }

  template <typename T>
  void Enumerate(EnumerateCallback<T> callback,
                 T param,
                 EnumerateOrder order) PA_NO_THREAD_SAFETY_ANALYSIS {
    if (order == kNormal) {
      PartitionRoot* root;
      for (root = Head(partition_roots_); root != nullptr;
           root = root->next_root) {
        callback(root, param);
      }
    } else {
      PA_DCHECK(order == kReverse);
      PartitionRoot* root;
      for (root = Tail(partition_roots_); root != nullptr;
           root = root->prev_root) {
        callback(root, param);
      }
    }
  }

  void Register(PartitionRoot* root) {
    internal::ScopedGuard guard(PartitionRoot::GetEnumeratorLock());
    root->next_root = partition_roots_;
    root->prev_root = nullptr;
    if (partition_roots_) {
      partition_roots_->prev_root = root;
    }
    partition_roots_ = root;
  }

  void Unregister(PartitionRoot* root) {
    internal::ScopedGuard guard(PartitionRoot::GetEnumeratorLock());
    PartitionRoot* prev = root->prev_root;
    PartitionRoot* next = root->next_root;
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

  PartitionRoot* Head(PartitionRoot* roots) { return roots; }

  PartitionRoot* Tail(PartitionRoot* roots) PA_NO_THREAD_SAFETY_ANALYSIS {
    if (!roots) {
      return nullptr;
    }
    PartitionRoot* node = roots;
    for (; node->next_root != nullptr; node = node->next_root)
      ;
    return node;
  }

  PartitionRoot* partition_roots_
      PA_GUARDED_BY(PartitionRoot::GetEnumeratorLock()) = nullptr;
};

}  // namespace internal

#endif  // PA_USE_PARTITION_ROOT_ENUMERATOR

#if (PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
     PA_CONFIG(HAS_ATFORK_HANDLER)) ||              \
    PA_CONFIG(ENABLE_SHADOW_METADATA)

namespace {

void LockRoot(PartitionRoot* root, bool) PA_NO_THREAD_SAFETY_ANALYSIS {
  PA_DCHECK(root);
  internal::PartitionRootLock(root).Acquire();
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

void UnlockOrReinitRoot(PartitionRoot* root,
                        bool in_child) PA_NO_THREAD_SAFETY_ANALYSIS {
  UnlockOrReinit(internal::PartitionRootLock(root), in_child);
}

}  // namespace

#endif  // (PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(HAS_ATFORK_HANDLER)) || PA_CONFIG(ENABLE_SHADOW_METADATA)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace {

#if PA_CONFIG(HAS_ATFORK_HANDLER)

// PA_NO_THREAD_SAFETY_ANALYSIS: acquires the lock and doesn't release it, by
// design.
void BeforeForkInParent() PA_NO_THREAD_SAFETY_ANALYSIS {
  //  PartitionRoot::GetLock() is private. So use
  //  g_root_enumerator_lock here.
  g_root_enumerator_lock.Acquire();
  internal::PartitionRootEnumerator::Instance().Enumerate(
      LockRoot, false,
      internal::PartitionRootEnumerator::EnumerateOrder::kNormal);

  ThreadCacheRegistry::GetLock().Acquire();
}

void ReleaseLocks(bool in_child) PA_NO_THREAD_SAFETY_ANALYSIS {
  // In reverse order, even though there are no lock ordering dependencies.
  UnlockOrReinit(ThreadCacheRegistry::GetLock(), in_child);
  internal::PartitionRootEnumerator::Instance().Enumerate(
      UnlockOrReinitRoot, in_child,
      internal::PartitionRootEnumerator::EnumerateOrder::kReverse);

  // PartitionRoot::GetLock() is private. So use
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

#if PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
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
#endif  // PA_BUILDFLAG(IS_LINUX) || PA_BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

#if PA_BUILDFLAG(IS_APPLE)
void PartitionAllocMallocHookOnBeforeForkInParent() {
  BeforeForkInParent();
}

void PartitionAllocMallocHookOnAfterForkInParent() {
  AfterForkInParent();
}

void PartitionAllocMallocHookOnAfterForkInChild() {
  AfterForkInChild();
}
#endif  // PA_BUILDFLAG(IS_APPLE)

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
namespace {

void MakeSuperPageExtentEntriesShared(PartitionRoot* root,
                                      internal::PoolHandleMask mask)
    PA_NO_THREAD_SAFETY_ANALYSIS {
  PA_DCHECK(root);
  // Regardless of root->ChoosePool(), no chance if shadow_pool_offset_ is
  // non-zero.
  if (root->settings.shadow_pool_offset_) {
    return;
  }

  switch (root->ChoosePool()) {
    case internal::kRegularPoolHandle:
      if (!ContainsFlags(mask, internal::PoolHandleMask::kRegular)) {
        return;
      }
      root->settings.shadow_pool_offset_ =
          internal::PartitionAddressSpace::RegularPoolShadowOffset();
      break;
    case internal::kBRPPoolHandle:
      if (!ContainsFlags(mask, internal::PoolHandleMask::kBRP)) {
        return;
      }
      root->settings.shadow_pool_offset_ =
          internal::PartitionAddressSpace::BRPPoolShadowOffset();
      break;
    case internal::kConfigurablePoolHandle:
      if (!ContainsFlags(mask, internal::PoolHandleMask::kConfigurable)) {
        return;
      }
      root->settings.shadow_pool_offset_ =
          internal::PartitionAddressSpace::ConfigurablePoolShadowOffset();
      break;
    default:
      return;
  }

  // For normal-bucketed.
  for (const internal::PartitionSuperPageExtentEntry<
           internal::MetadataKind::kReadOnly>* extent = root->first_extent;
       extent != nullptr; extent = extent->next) {
    //  The page which contains the extent is in-used and shared mapping.
    uintptr_t super_page = SuperPagesBeginFromExtent(extent);
    for (size_t i = 0; i < extent->number_of_consecutive_super_pages; ++i) {
      internal::PartitionAddressSpace::MapMetadata(super_page,
                                                   /*copy_metadata=*/true);
      super_page += kSuperPageSize;
    }
    PA_DCHECK(extent->root == root);
  }

  // For direct-mapped.
  for (const internal::PartitionDirectMapExtent<
           internal::MetadataKind::kReadOnly>* extent = root->direct_map_list;
       extent != nullptr; extent = extent->next_extent) {
    internal::PartitionAddressSpace::MapMetadata(
        reinterpret_cast<uintptr_t>(extent) & internal::kSuperPageBaseMask,
        /*copy_metadata=*/true);
  }
}

}  // namespace
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

namespace internal {

namespace {
// 64 was chosen arbitrarily, as it seems like a reasonable trade-off between
// performance and purging opportunity. Higher value (i.e. smaller slots)
// wouldn't necessarily increase chances of purging, but would result in
// more work and larger |slot_usage| array. Lower value would probably decrease
// chances of purging. Not empirically tested.
constexpr size_t kMaxPurgeableSlotsPerSystemPage = 64;
// See above, this will lead to less work getting done, so lower cost, lower
// savings.
constexpr size_t kConservativeMaxPurgeableSlotsPerSystemPage = 2;
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MinPurgeableSlotSize() {
  return SystemPageSize() / kMaxPurgeableSlotsPerSystemPage;
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MinConservativePurgeableSlotSize() {
  return SystemPageSize() / kConservativeMaxPurgeableSlotsPerSystemPage;
}
}  // namespace

// The function attempts to unprovision unused slots and discard unused pages.
// It may also "straighten" the free list.
//
// If `accounting_only` is set to true, no action is performed and the function
// merely returns the number of bytes in the would-be discarded pages.
PA_NOPROFILE
static size_t PartitionPurgeSlotSpan(
    PartitionRoot* root,
    internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>* slot_span,
    bool accounting_only)
    PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(root)) {
  const internal::PartitionBucket* bucket = slot_span->bucket;
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
    if (discardable_bytes && !accounting_only) {
      uintptr_t slot_span_start = internal::SlotSpanMetadata<
          internal::MetadataKind::kReadOnly>::ToSlotSpanStart(slot_span);
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
#elif PA_BUILDFLAG(IS_APPLE) || \
    defined(PARTITION_ALLOCATOR_CONSTANTS_POSIX_NONCONST_PAGE_SIZE)
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
  size_t num_provisioned_slots =
      bucket_num_slots - slot_span->num_unprovisioned_slots;
  char slot_usage[kMaxSlotCount];
#if !PA_BUILDFLAG(IS_WIN)
  // The last freelist entry should not be discarded when using OS_WIN.
  // DiscardVirtualMemory makes the contents of discarded memory undefined.
  size_t last_slot = static_cast<size_t>(-1);
#endif
  memset(slot_usage, 1, num_provisioned_slots);
  uintptr_t slot_span_start = internal::SlotSpanMetadata<
      internal::MetadataKind::kReadOnly>::ToSlotSpanStart(slot_span);
  // First, walk the freelist for this slot span and make a bitmap of which
  // slots are not in use.
  const PartitionFreelistDispatcher* freelist_dispatcher =
      root->get_freelist_dispatcher();

  for (PartitionFreelistEntry* entry = slot_span->get_freelist_head(); entry;
       entry = freelist_dispatcher->GetNext(entry, slot_size)) {
    size_t slot_number =
        bucket->GetSlotNumber(SlotStartPtr2Addr(entry) - slot_span_start);
    PA_DCHECK(slot_number < num_provisioned_slots);
    slot_usage[slot_number] = 0;
#if !PA_BUILDFLAG(IS_WIN)
    // If we have a slot where the encoded next pointer is 0, we can actually
    // discard that entry because touching a discarded page is guaranteed to
    // return the original content or 0. (Note that this optimization won't be
    // effective on big-endian machines because the masking function is
    // negation.)
    if (freelist_dispatcher->IsEncodedNextPtrZero(entry)) {
      last_slot = slot_number;
    }
#endif
  }

  // If the slot(s) at the end of the slot span are not in use, we can truncate
  // them entirely and rewrite the freelist.
  size_t truncated_slots = 0;
  while (!slot_usage[num_provisioned_slots - 1]) {
    truncated_slots++;
    num_provisioned_slots--;
    PA_DCHECK(num_provisioned_slots);
  }
  // First, do the work of calculating the discardable bytes. Don't actually
  // discard anything if `accounting_only` is set.
  size_t unprovisioned_bytes = 0;
  uintptr_t begin_addr = slot_span_start + (num_provisioned_slots * slot_size);
  uintptr_t end_addr = begin_addr + (slot_size * truncated_slots);
  if (truncated_slots) {
    // The slots that do not contain discarded pages should not be included to
    // |truncated_slots|. Detects those slots and fixes |truncated_slots| and
    // |num_provisioned_slots| accordingly.
    uintptr_t rounded_up_truncatation_begin_addr =
        RoundUpToSystemPage(begin_addr);
    while (begin_addr + slot_size <= rounded_up_truncatation_begin_addr) {
      begin_addr += slot_size;
      PA_DCHECK(truncated_slots);
      --truncated_slots;
      ++num_provisioned_slots;
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
  }

  // If `accounting_only` isn't set, then take action to remove unprovisioned
  // slots from the free list (if any) and "straighten" the list (if
  // requested) to help reduce fragmentation in the future. Then
  // discard/decommit the pages hosting the unprovisioned slots.
  if (!accounting_only) {
    auto straighten_mode =
        PartitionRoot::GetStraightenLargerSlotSpanFreeListsMode();
    bool straighten =
        straighten_mode == StraightenLargerSlotSpanFreeListsMode::kAlways ||
        (straighten_mode ==
             StraightenLargerSlotSpanFreeListsMode::kOnlyWhenUnprovisioning &&
         unprovisioned_bytes);

    PA_DCHECK((unprovisioned_bytes > 0) == (truncated_slots > 0));
    size_t new_unprovisioned_slots =
        truncated_slots + slot_span->num_unprovisioned_slots;
    PA_DCHECK(new_unprovisioned_slots <= bucket->get_slots_per_span());
    slot_span->ToWritable(root)->num_unprovisioned_slots =
        new_unprovisioned_slots;

    size_t num_new_freelist_entries = 0;
    internal::PartitionFreelistEntry* back = nullptr;
    if (straighten) {
      // Rewrite the freelist to "straighten" it. This achieves two things:
      // getting rid of unprovisioned entries, ordering etnries based on how
      // close they're to the slot span start. This reduces chances of
      // allocating further slots, in hope that we'll get some unused pages at
      // the end of the span that can be unprovisioned, thus reducing
      // fragmentation.
      for (size_t slot_index = 0; slot_index < num_provisioned_slots;
           ++slot_index) {
        if (slot_usage[slot_index]) {
          continue;
        }
        // Add the slot to the end of the list. The most proper thing to do
        // would be to null-terminate the new entry with:
        //   auto* entry = PartitionFreelistEntry::EmplaceAndInitNull(
        //       slot_span_start + (slot_size * slot_index));
        // But no need to do this, as it's last-ness is likely temporary, and
        // the next iteration's back->SetNext(), or the post-loop
        // PartitionFreelistEntry::EmplaceAndInitNull(back) will override it
        // anyway.
        auto* entry = static_cast<PartitionFreelistEntry*>(
            SlotStartAddr2Ptr(slot_span_start + (slot_size * slot_index)));
        if (num_new_freelist_entries) {
          freelist_dispatcher->SetNext(back, entry);
        } else {
          slot_span->ToWritable(root)->SetFreelistHead(entry, root);
        }
        back = entry;
        num_new_freelist_entries++;
      }
    } else if (unprovisioned_bytes) {
      // If there are any unprovisioned entries, scan the list to remove them,
      // without "straightening" it.
      uintptr_t first_unprovisioned_slot =
          slot_span_start + (num_provisioned_slots * slot_size);
      bool skipped = false;
      for (PartitionFreelistEntry* entry = slot_span->get_freelist_head();
           entry; entry = freelist_dispatcher->GetNext(entry, slot_size)) {
        uintptr_t entry_addr = SlotStartPtr2Addr(entry);
        if (entry_addr >= first_unprovisioned_slot) {
          skipped = true;
          continue;
        }
        // If the last visited entry was skipped (due to being unprovisioned),
        // update the next pointer of the last not skipped entry (or the head
        // if no entry exists). Otherwise the link is already correct.
        if (skipped) {
          if (num_new_freelist_entries) {
            freelist_dispatcher->SetNext(back, entry);
          } else {
            slot_span->ToWritable(root)->SetFreelistHead(entry, root);
          }
          skipped = false;
        }
        back = entry;
        num_new_freelist_entries++;
      }
    }
    // If any of the above loops were executed, null-terminate the last entry,
    // or the head if no entry exists.
    if (straighten || unprovisioned_bytes) {
      if (num_new_freelist_entries) {
        PA_DCHECK(back);
        freelist_dispatcher->EmplaceAndInitNull(back);
#if !PA_BUILDFLAG(IS_WIN)
        // Memorize index of the last slot in the list, as it may be able to
        // participate in an optimization related to page discaring (below), due
        // to its next pointer encoded as 0.
        last_slot =
            bucket->GetSlotNumber(SlotStartPtr2Addr(back) - slot_span_start);
#endif
      } else {
        PA_DCHECK(!back);
        slot_span->ToWritable(root)->SetFreelistHead(nullptr, root);
      }
      PA_DCHECK(num_new_freelist_entries ==
                num_provisioned_slots - slot_span->num_allocated_slots);
    }

#if PA_BUILDFLAG(USE_FREESLOT_BITMAP)
    FreeSlotBitmapReset(slot_span_start + (slot_size * num_provisioned_slots),
                        end_addr, slot_size);
#endif

    if (unprovisioned_bytes) {
      if (!kUseLazyCommit) {
        // Discard the memory.
        ScopedSyscallTimer timer{root};
        DiscardSystemPages(begin_addr, unprovisioned_bytes);
      } else {
        // See crbug.com/1431606 to understand the detail. LazyCommit depends
        // on the design: both used slots and unused slots (=in the freelist)
        // are committed. However this removes the unused slots from the
        // freelist. So if using DiscardSystemPages() here, PartitionAlloc may
        // commit the system pages which has been already committed again.
        // This will make commited_size and max_committed_size metrics wrong.
        // PA should use DecommitSystemPagesForData() instead.
        root->DecommitSystemPagesForData(
            begin_addr, unprovisioned_bytes,
            PageAccessibilityDisposition::kAllowKeepForPerf);
      }
    }
  }

  if (slot_size < SystemPageSize()) {
    // Returns here because implementing the following steps for smaller slot
    // size will need a complicated logic and make the code messy.
    return discardable_bytes;
  }

  // Next, walk the slots and for any not in use, consider which system pages
  // are no longer needed. We can discard any system pages back to the system as
  // long as we don't interfere with a freelist pointer or an adjacent used
  // slot. Note they'll be automatically paged back in when touched, and
  // zero-initialized (except Windows).
  for (size_t i = 0; i < num_provisioned_slots; ++i) {
    if (slot_usage[i]) {
      continue;
    }

    // The first address we can safely discard is just after the freelist
    // pointer. There's one optimization opportunity: if the freelist pointer is
    // encoded as 0, we can discard that pointer value too (except on
    // Windows).
    begin_addr = slot_span_start + (i * slot_size);
    end_addr = begin_addr + slot_size;
    bool can_discard_free_list_pointer = false;
#if !PA_BUILDFLAG(IS_WIN)
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
      if (!accounting_only) {
        // Discard the pages. But don't be tempted to decommit it (as done
        // above), because here we're getting rid of provisioned pages amidst
        // used pages, so we're relying on them to materialize automatically
        // when the virtual address is accessed, so the mapping needs to be
        // intact.
        ScopedSyscallTimer timer{root};
        DiscardSystemPages(begin_addr, partial_slot_bytes);
      }
    }
  }

  return discardable_bytes;
}

PA_NOPROFILE
static void PartitionPurgeBucket(PartitionRoot* root,
                                 internal::PartitionBucket* bucket)
    PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(root)) {
  if (bucket->active_slot_spans_head !=
      internal::SlotSpanMetadata<
          internal::MetadataKind::kReadOnly>::get_sentinel_slot_span()) {
    for (internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>*
             slot_span = bucket->active_slot_spans_head;
         slot_span; slot_span = slot_span->next_slot_span) {
      PA_DCHECK(
          slot_span !=
          internal::SlotSpanMetadata<
              internal::MetadataKind::kReadOnly>::get_sentinel_slot_span());
      PartitionPurgeSlotSpan(root, slot_span, false);
    }
  }
}

static void PartitionDumpSlotSpanStats(
    PartitionBucketMemoryStats* stats_out,
    PartitionRoot* root,
    internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>* slot_span)
    PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(root)) {
  uint16_t bucket_num_slots = slot_span->bucket->get_slots_per_span();

  if (slot_span->is_decommitted()) {
    ++stats_out->num_decommitted_slot_spans;
    return;
  }

  stats_out->discardable_bytes += PartitionPurgeSlotSpan(root, slot_span, true);

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

static void PartitionDumpBucketStats(PartitionBucketMemoryStats* stats_out,
                                     PartitionRoot* root,
                                     const internal::PartitionBucket* bucket)
    PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(root)) {
  PA_DCHECK(!bucket->is_direct_mapped());
  stats_out->is_valid = false;
  // If the active slot span list is empty (==internal::SlotSpanMetadata<
  // internal::MetadataKind::kReadOnly>::get_sentinel_slot_span()),
  // the bucket might still need to be reported if it has a list of empty,
  // decommitted or full slot spans.
  if (bucket->active_slot_spans_head ==
          internal::SlotSpanMetadata<
              internal::MetadataKind::kReadOnly>::get_sentinel_slot_span() &&
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

  for (internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>*
           slot_span = bucket->empty_slot_spans_head;
       slot_span; slot_span = slot_span->next_slot_span) {
    PA_DCHECK(slot_span->is_empty() || slot_span->is_decommitted());
    PartitionDumpSlotSpanStats(stats_out, root, slot_span);
  }
  for (internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>*
           slot_span = bucket->decommitted_slot_spans_head;
       slot_span; slot_span = slot_span->next_slot_span) {
    PA_DCHECK(slot_span->is_decommitted());
    PartitionDumpSlotSpanStats(stats_out, root, slot_span);
  }

  if (bucket->active_slot_spans_head !=
      internal::SlotSpanMetadata<
          internal::MetadataKind::kReadOnly>::get_sentinel_slot_span()) {
    for (internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>*
             slot_span = bucket->active_slot_spans_head;
         slot_span; slot_span = slot_span->next_slot_span) {
      PA_DCHECK(
          slot_span !=
          internal::SlotSpanMetadata<
              internal::MetadataKind::kReadOnly>::get_sentinel_slot_span());
      PartitionDumpSlotSpanStats(stats_out, root, slot_span);
    }
  }
}

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
void DCheckIfManagedByPartitionAllocBRPPool(uintptr_t address) {
  PA_DCHECK(IsManagedByPartitionAllocBRPPool(address));
}
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
void PartitionAllocThreadIsolationInit(ThreadIsolationOption thread_isolation) {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  ThreadIsolationSettings::settings.enabled = true;
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
  PartitionAddressSpace::InitThreadIsolatedPool(thread_isolation);
  // Call WriteProtectThreadIsolatedGlobals last since we might not have write
  // permissions to to globals afterwards.
  WriteProtectThreadIsolatedGlobals(thread_isolation);
}
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

}  // namespace internal

[[noreturn]] PA_NOINLINE void PartitionRoot::OutOfMemory(size_t size) {
  const size_t virtual_address_space_size =
      total_size_of_super_pages.load(std::memory_order_relaxed) +
      total_size_of_direct_mapped_pages.load(std::memory_order_relaxed);
#if !PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
  const size_t uncommitted_size =
      virtual_address_space_size -
      total_size_of_committed_pages.load(std::memory_order_relaxed);

  // Check whether this OOM is due to a lot of super pages that are allocated
  // but not committed, probably due to http://crbug.com/421387.
  if (uncommitted_size > internal::kReasonableSizeOfUnusedPages) {
    internal::PartitionOutOfMemoryWithLotsOfUncommitedPages(size);
  }

#if PA_BUILDFLAG(IS_WIN)
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
  PA_DEBUG_DATA_ON_STACK("iswow64", static_cast<size_t>(is_wow_64));
#else
  constexpr size_t kReasonableVirtualSize =
      // 1.5GiB elsewhere, since address space is typically 3GiB.
      (1024 + 512) * 1024 * 1024;
#endif
  if (virtual_address_space_size > kReasonableVirtualSize) {
    internal::PartitionOutOfMemoryWithLargeVirtualSize(
        virtual_address_space_size);
  }
#endif  // #if !PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)

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

void PartitionRoot::DecommitEmptySlotSpans() {
  ShrinkEmptySlotSpansRing(0);
  // Just decommitted everything, and holding the lock, should be exactly 0.
  PA_DCHECK(empty_slot_spans_dirty_bytes == 0);
}

void PartitionRoot::DecommitEmptySlotSpansForTesting() {
  ::partition_alloc::internal::ScopedGuard guard{
      internal::PartitionRootLock(this)};
  DecommitEmptySlotSpans();
}

void PartitionRoot::DestructForTesting()
    PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this)) {
  // We need to destruct the thread cache before we unreserve any of the super
  // pages below, which we currently are not doing. So, we should only call
  // this function on PartitionRoots without a thread cache.
  PA_CHECK(!settings.with_thread_cache);
  auto pool_handle = ChoosePool();
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // The pages managed by thread isolated pool will be free-ed at
  // UninitThreadIsolatedForTesting(). Don't invoke FreePages() for the pages.
  if (pool_handle == internal::kThreadIsolatedPoolHandle) {
    return;
  }
  PA_DCHECK(pool_handle < internal::kNumPools);
#else
  PA_DCHECK(pool_handle <= internal::kNumPools);
#endif

  {
    auto* curr = first_extent;
    while (curr != nullptr) {
      auto* next = curr->next;
      uintptr_t address = SuperPagesBeginFromExtent(curr);
      size_t size =
          internal::kSuperPageSize * curr->number_of_consecutive_super_pages;
#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
      internal::AddressPoolManager::GetInstance().MarkUnused(pool_handle,
                                                             address, size);
#endif
      internal::AddressPoolManager::GetInstance().UnreserveAndDecommit(
          pool_handle, address, size);
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
      if (internal::PartitionAddressSpace::IsShadowMetadataEnabled(
              pool_handle)) {
        internal::PartitionAddressSpace::UnmapShadowMetadata(address,
                                                             pool_handle);
      }
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
      curr = next;
    }
    first_extent = current_extent = nullptr;
  }
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  // Decommit direct-mapped allocations too.
  if (internal::PartitionAddressSpace::IsShadowMetadataEnabled(pool_handle)) {
    auto* curr = direct_map_list;
    while (curr != nullptr) {
      auto* next = curr->next_extent;
      uintptr_t reservation_start = internal::base::bits::AlignDown(
          reinterpret_cast<uintptr_t>(curr), kSuperPageSize);
      size_t reservation_size = curr->reservation_size;

      {
        uintptr_t reservation_end = reservation_start + reservation_size;
        auto* offset_ptr =
            internal::ReservationOffsetPointer(reservation_start);
        // Reset the offset table entries for the given memory before
        // unreserving it. Since the memory is not unreserved and not available
        // for other threads, the table entries for the memory are not modified
        // by other threads either. So we can update the table entries without
        // race condition.
        uint16_t i = 0;
        for (uintptr_t address = reservation_start; address < reservation_end;
             address += kSuperPageSize) {
          PA_DCHECK(offset_ptr <
                    internal::GetReservationOffsetTableEnd(address));
          PA_DCHECK(*offset_ptr == i++);
          *offset_ptr++ = internal::kOffsetTagNotAllocated;
        }
      }
#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
      internal::AddressPoolManager::GetInstance().MarkUnused(
          pool_handle, reservation_start, reservation_size);
#endif  // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

      // After resetting the table entries, unreserve and decommit the memory.
      internal::AddressPoolManager::GetInstance().UnreserveAndDecommit(
          pool_handle, reservation_start, reservation_size);

      internal::PartitionAddressSpace::UnmapShadowMetadata(reservation_start,
                                                           pool_handle);
      curr = next;
    }
    direct_map_list = nullptr;
  }
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
}

#if PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)
void PartitionRoot::InitMac11MallocSizeHackUsableSize() {
  settings.mac11_malloc_size_hack_enabled_ = true;

  // Request of 32B will fall into a 48B bucket in the presence of BRP
  // in-slot metadata, yielding |48 - in_slot_metadata_size| of actual usable
  // space.
  PA_DCHECK(settings.in_slot_metadata_size);
  settings.mac11_malloc_size_hack_usable_size_ =
      48 - settings.in_slot_metadata_size;
}

void PartitionRoot::EnableMac11MallocSizeHackForTesting() {
  InitMac11MallocSizeHackUsableSize();
}

void PartitionRoot::EnableMac11MallocSizeHackIfNeeded() {
  PA_DCHECK(settings.brp_enabled_);
  if (internal::base::mac::MacOSMajorVersion() == 11) {
    InitMac11MallocSizeHackUsableSize();
  }
}
#endif  // PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && \
    !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
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
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) &&
        // !PA_BUILDFLAG(HAS_64_BIT_POINTERS)

void PartitionRoot::Init(PartitionOptions opts) {
  {
#if PA_BUILDFLAG(IS_APPLE)
    // Needed to statically bound page size, which is a runtime constant on
    // apple OSes.
    PA_CHECK((internal::SystemPageSize() == (size_t{1} << 12)) ||
             (internal::SystemPageSize() == (size_t{1} << 14)));
#elif PA_BUILDFLAG(IS_LINUX) && PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
    // Check runtime pagesize. Though the code is currently the same, it is
    // not merged with the IS_APPLE case above as a 1 << 16 case is only
    // supported on Linux on AArch64.
    PA_CHECK((internal::SystemPageSize() == (size_t{1} << 12)) ||
             (internal::SystemPageSize() == (size_t{1} << 14)) ||
             (internal::SystemPageSize() == (size_t{1} << 16)));
#endif

    ::partition_alloc::internal::ScopedGuard guard{lock_};
    if (initialized) {
      return;
    }

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    // Reserve address space for PartitionAlloc.
    internal::PartitionAddressSpace::Init();
#endif

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && \
    !PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    ReserveBackupRefPtrGuardRegionIfNeeded();
#endif

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    settings.brp_enabled_ = opts.backup_ref_ptr == PartitionOptions::kEnabled;
#else   // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_CHECK(opts.backup_ref_ptr == PartitionOptions::kDisabled);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    settings.use_configurable_pool =
        (opts.use_configurable_pool == PartitionOptions::kAllowed) &&
        IsConfigurablePoolAvailable();
    PA_DCHECK(!settings.use_configurable_pool || IsConfigurablePoolAvailable());
    settings.zapping_by_free_flags =
        opts.zapping_by_free_flags == PartitionOptions::kEnabled;

    settings.scheduler_loop_quarantine =
        opts.scheduler_loop_quarantine == PartitionOptions::kEnabled;
    if (settings.scheduler_loop_quarantine) {
      internal::LightweightQuarantineBranchConfig global_config = {
          .lock_required = true,
          .branch_capacity_in_bytes =
              opts.scheduler_loop_quarantine_branch_capacity_in_bytes,
      };
      scheduler_loop_quarantine_branch_capacity_in_bytes =
          opts.scheduler_loop_quarantine_branch_capacity_in_bytes;
      scheduler_loop_quarantine.emplace(
          scheduler_loop_quarantine_root.CreateBranch(global_config));
    } else {
      // Deleting a running quarantine is not supported.
      PA_CHECK(!scheduler_loop_quarantine.has_value());
    }

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    settings.memory_tagging_enabled_ =
        opts.memory_tagging.enabled == PartitionOptions::kEnabled;
    // Memory tagging is not supported in the configurable pool because MTE
    // stores tagging information in the high bits of the pointer, it causes
    // issues with components like V8's ArrayBuffers which use custom pointer
    // representations. All custom representations encountered so far rely on an
    // "is in configurable pool?" check, so we use that as a proxy.
    PA_CHECK(!settings.memory_tagging_enabled_ ||
             !settings.use_configurable_pool);

    settings.use_random_memory_tagging_ =
        opts.memory_tagging.random_memory_tagging == PartitionOptions::kEnabled;

    settings.memory_tagging_reporting_mode_ =
        opts.memory_tagging.reporting_mode;
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

    settings.use_pool_offset_freelists =
        opts.use_pool_offset_freelists == PartitionOptions::kEnabled;

    // brp_enabled() is not supported in the configurable pool because
    // BRP requires objects to be in a different Pool.
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_CHECK(!(settings.use_configurable_pool && brp_enabled()));
#endif

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    // BRP and thread isolated mode use different pools, so they can't be
    // enabled at the same time.
    PA_CHECK(!opts.thread_isolation.enabled ||
             opts.backup_ref_ptr == PartitionOptions::kDisabled);
    settings.thread_isolation = opts.thread_isolation;
#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#if PA_CONFIG(EXTRAS_REQUIRED)
    settings.extras_size = 0;

    if (settings.use_cookie) {
      settings.extras_size += internal::kPartitionCookieSizeAdjustment;
    }

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    if (brp_enabled()) {
      settings.in_slot_metadata_size = internal::kInSlotMetadataSizeAdjustment;
      settings.extras_size += internal::kInSlotMetadataSizeAdjustment;
#if PA_CONFIG(MAYBE_ENABLE_MAC11_MALLOC_SIZE_HACK)
      EnableMac11MallocSizeHackIfNeeded();
#endif
    }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#endif  // PA_CONFIG(EXTRAS_REQUIRED)

    // We mark the sentinel slot span as free to make sure it is skipped by our
    // logic to find a new active slot span.
    memset(&sentinel_bucket, 0, sizeof(sentinel_bucket));
    sentinel_bucket.active_slot_spans_head = internal::SlotSpanMetadata<
        internal::MetadataKind::kReadOnly>::get_sentinel_slot_span_non_const();

    // This is a "magic" value so we can test if a root pointer is valid.
    inverted_self = ~reinterpret_cast<uintptr_t>(this);

    const bool use_small_single_slot_spans =
        opts.use_small_single_slot_spans == PartitionOptions::kEnabled;

    // Set up the actual usable buckets first.
    constexpr internal::BucketIndexLookup lookup{};
    size_t bucket_index = 0;
    while (lookup.bucket_sizes()[bucket_index] !=
           internal::kInvalidBucketSize) {
      buckets[bucket_index].Init(lookup.bucket_sizes()[bucket_index],
                                 use_small_single_slot_spans);
      bucket_index++;
    }
    PA_DCHECK(bucket_index < internal::kNumBuckets);

    // Remaining buckets are not usable, and not real.
    for (size_t index = bucket_index; index < internal::kNumBuckets; index++) {
      // Cannot init with size 0 since it computes 1 / size, but make sure the
      // bucket is invalid.
      buckets[index].Init(internal::kInvalidBucketSize,
                          use_small_single_slot_spans);
      buckets[index].active_slot_spans_head = nullptr;
      PA_DCHECK(!buckets[index].is_valid());
    }

#if !PA_CONFIG(THREAD_CACHE_SUPPORTED)
    // TLS in ThreadCache not supported on other OSes.
    settings.with_thread_cache = false;
#else
    ThreadCache::EnsureThreadSpecificDataInitialized();
    settings.with_thread_cache =
        (opts.thread_cache == PartitionOptions::kEnabled);

    if (settings.with_thread_cache) {
      ThreadCache::Init(this);
    }
#endif  // !PA_CONFIG(THREAD_CACHE_SUPPORTED)

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)
    internal::PartitionRootEnumerator::Instance().Register(this);
#endif

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    if (internal::PartitionAddressSpace::IsShadowMetadataEnabled(
            ChoosePool())) {
      switch (ChoosePool()) {
        case internal::kRegularPoolHandle:
          settings.shadow_pool_offset_ =
              internal::PartitionAddressSpace::RegularPoolShadowOffset();
          break;
        case internal::kBRPPoolHandle:
          settings.shadow_pool_offset_ =
              internal::PartitionAddressSpace::BRPPoolShadowOffset();
          break;
        case internal::kConfigurablePoolHandle:
          settings.shadow_pool_offset_ =
              internal::PartitionAddressSpace::ConfigurablePoolShadowOffset();
          break;
        default:
          break;
      }
    }
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

    initialized = true;
  }

  // Called without the lock, might allocate.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PartitionAllocMallocInitOnce();
#endif

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  if (settings.thread_isolation.enabled) {
    internal::PartitionAllocThreadIsolationInit(settings.thread_isolation);
  }
#endif
}

PartitionRoot::Settings::Settings() = default;

PartitionRoot::PartitionRoot() : scheduler_loop_quarantine_root(*this) {}

PartitionRoot::PartitionRoot(PartitionOptions opts)
    : scheduler_loop_quarantine_root(*this) {
  Init(opts);
}

PartitionRoot::~PartitionRoot() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(!settings.with_thread_cache)
      << "Must not destroy a partition with a thread cache";
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)
  if (initialized) {
    internal::PartitionRootEnumerator::Instance().Unregister(this);
  }
#endif  // PA_CONFIG(USE_PARTITION_ALLOC_ENUMERATOR)
}

void PartitionRoot::EnableThreadCacheIfSupported() {
#if PA_CONFIG(THREAD_CACHE_SUPPORTED)
  ::partition_alloc::internal::ScopedGuard guard{lock_};
  PA_CHECK(!settings.with_thread_cache);
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
  settings.with_thread_cache = true;
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)
}

bool PartitionRoot::TryReallocInPlaceForDirectMap(
    internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>* slot_span,
    size_t requested_size) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  // Slot-span metadata isn't MTE-tagged.
  PA_DCHECK(
      internal::IsManagedByDirectMap(reinterpret_cast<uintptr_t>(slot_span)));

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  auto* extent = ReadOnlyDirectMapExtent::FromSlotSpanMetadata(slot_span);
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
  size_t current_usable_size = GetSlotUsableSize(slot_span);
  uintptr_t slot_start = internal::SlotSpanMetadata<
      internal::MetadataKind::kReadOnly>::ToSlotSpanStart(slot_span);
  // This is the available part of the reservation up to which the new
  // allocation can grow.
  size_t available_reservation_size =
      current_reservation_size - extent->padding_for_alignment -
      PartitionRoot::GetDirectMapMetadataAndGuardPagesSize();
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  uintptr_t reservation_start = slot_start & internal::kSuperPageBaseMask;
  PA_DCHECK(internal::IsReservationStart(reservation_start));
  PA_DCHECK(slot_start + available_reservation_size ==
            reservation_start + current_reservation_size -
                GetDirectMapMetadataAndGuardPagesSize() +
                internal::PartitionPageSize());
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

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
    // Grow within the actually reserved address space. Just need to make sure
    // the pages are accessible.
    size_t recommit_slot_size_growth = new_slot_size - current_slot_size;
    // Direct map never uses tagging, as size is always >kMaxMemoryTaggingSize.
    RecommitSystemPagesForData(
        slot_start + current_slot_size, recommit_slot_size_growth,
        PageAccessibilityDisposition::kRequireUpdate, false);
    // The recommited system pages had been already reserved and all the
    // entries in the reservation offset table (for entire reservation_size
    // region) have been already initialized.

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
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
  slot_span->ToWritable(this)->SetRawSize(raw_size);
#if !PA_CONFIG(ENABLE_SHADOW_METADATA)
  slot_span->bucket->slot_size = new_slot_size;
#else
  internal::PartitionBucket* writable_bucket =
      reinterpret_cast<internal::PartitionBucket*>(
          reinterpret_cast<intptr_t>(slot_span->bucket) + ShadowPoolOffset());
  writable_bucket->slot_size = new_slot_size;
#endif  // !PA_CONFIG(ENABLE_SHADOW_METADATA)
  IncreaseTotalSizeOfAllocatedBytes(reinterpret_cast<uintptr_t>(slot_span),
                                    slot_span->bucket->slot_size, raw_size);

  // Always record in-place realloc() as free()+malloc() pair.
  //
  // The early returns above (`return false`) will fall back to free()+malloc(),
  // so this is consistent.
  auto* thread_cache = GetOrCreateThreadCache();
  if (ThreadCache::IsValid(thread_cache)) {
    thread_cache->RecordDeallocation(current_usable_size);
    thread_cache->RecordAllocation(GetSlotUsableSize(slot_span));
  }

  // Write a new trailing cookie.
  if (settings.use_cookie) {
    auto* object = static_cast<unsigned char*>(SlotStartToObject(slot_start));
    internal::PartitionCookieWriteValue(object + GetSlotUsableSize(slot_span));
  }

  return true;
}

bool PartitionRoot::TryReallocInPlaceForNormalBuckets(
    void* object,
    internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>* slot_span,
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
  size_t current_usable_size = GetSlotUsableSize(slot_span);

  // Trying to allocate |new_size| would use the same amount of underlying
  // memory as we're already using, so re-use the allocation after updating
  // statistics (and cookie, if present).
  if (slot_span->CanStoreRawSize()) {
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && PA_BUILDFLAG(DCHECKS_ARE_ON)
    internal::InSlotMetadata* old_ref_count = nullptr;
    if (brp_enabled()) [[likely]] {
      old_ref_count = InSlotMetadataPointerFromSlotStartAndSize(
          slot_start, slot_span->bucket->slot_size);
    }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) &&
        // PA_BUILDFLAG(DCHECKS_ARE_ON)
    size_t new_raw_size = AdjustSizeForExtrasAdd(new_size);
    slot_span->ToWritable(this)->SetRawSize(new_raw_size);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) && PA_BUILDFLAG(DCHECKS_ARE_ON)
    if (brp_enabled()) [[likely]] {
      internal::InSlotMetadata* new_ref_count =
          InSlotMetadataPointerFromSlotStartAndSize(
              slot_start, slot_span->bucket->slot_size);
      PA_DCHECK(new_ref_count == old_ref_count);
    }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT) &&
        // PA_BUILDFLAG(DCHECKS_ARE_ON)
    // Write a new trailing cookie only when it is possible to keep track
    // raw size (otherwise we wouldn't know where to look for it later).
    if (settings.use_cookie) {
      internal::PartitionCookieWriteValue(static_cast<unsigned char*>(object) +
                                          GetSlotUsableSize(slot_span));
    }
  }

  // Always record a realloc() as a free() + malloc(), even if it's in
  // place. When we cannot do it in place (`return false` above), the allocator
  // falls back to free()+malloc(), so this is consistent.
  ThreadCache* thread_cache = GetOrCreateThreadCache();
  if (ThreadCache::IsValid(thread_cache)) [[likely]] {
    thread_cache->RecordDeallocation(current_usable_size);
    thread_cache->RecordAllocation(GetSlotUsableSize(slot_span));
  }

  return object;
}

void PartitionRoot::PurgeMemory(int flags) {
  auto start = now_maybe_overridden_for_testing();
  unsigned int local_purge_generation, local_purge_next_bucket_index;

  {
    ::partition_alloc::internal::ScopedGuard guard{
        internal::PartitionRootLock(this)};
    local_purge_next_bucket_index = purge_next_bucket_index;
    local_purge_generation = purge_generation;

    if (flags & PurgeFlags::kDecommitEmptySlotSpans) {
      DecommitEmptySlotSpans();

      if (flags & PurgeFlags::kLimitDuration &&
          (now_maybe_overridden_for_testing() - start > kMaxPurgeDuration)) {
        return;
      }
    }
  }

  if (flags & PurgeFlags::kDiscardUnusedSystemPages) {
    // Don't do the most expensive operation except for the largest buckets,
    // where the cost of doing so is lower, and gains are likely higher,
    // except in two cases
    // - We don't care about reclaim duration
    // - It's been a long time (16 walks through the entire bucket list)
    //
    // Note that in the latter case, we still limit total reclaim duration.
    size_t min_bucket_size_to_purge =
        internal::MinConservativePurgeableSlotSize();
    if (!(flags & PurgeFlags::kLimitDuration) || !local_purge_generation) {
      min_bucket_size_to_purge = internal::MinPurgeableSlotSize();
    }

    for (unsigned int bucket_index = local_purge_next_bucket_index;
         bucket_index < internal::kNumBuckets; bucket_index++) {
      // Only acquire the lock for a single iteration, so that if there is a
      // waiter blocked on it, it can steal it from us before the next
      // one.
      ::partition_alloc::internal::ScopedGuard guard{
          internal::PartitionRootLock(this)};

      Bucket& bucket = buckets[bucket_index];
      if (bucket.slot_size == internal::kInvalidBucketSize) {
        continue;
      }

      if (bucket.slot_size >= min_bucket_size_to_purge) {
        internal::PartitionPurgeBucket(this, &bucket);
      } else {
        if (sort_smaller_slot_span_free_lists_) {
          bucket.SortSmallerSlotSpanFreeLists(this);
        }
      }

      // Do it at the end, as the actions above change the status of slot
      // spans (e.g. empty -> decommitted).
      bucket.MaintainActiveList(this);

      if (sort_active_slot_spans_) {
        bucket.SortActiveSlotSpans(this);
      }
      // Checking at the end to make sure we make progress by processing at
      // least one bucket.
      if (flags & PurgeFlags::kLimitDuration &&
          (now_maybe_overridden_for_testing() - start > kMaxPurgeDuration)) {
        // Pick up where we stopped next time.
        purge_next_bucket_index = (bucket_index + 1) % kNumBuckets;
        return;
      }
    }

    {
      ::partition_alloc::internal::ScopedGuard guard{
          internal::PartitionRootLock(this)};
      // In theory, these may have been modified since we last read them into
      // the local variables at the beginning of the function. This should not
      // happen (since Purge() runs on a single thread), and also does not
      // matter since we just want to make sure to not do too much work and to
      // make some progress.
      purge_next_bucket_index = 0;
      purge_generation = (purge_generation + 1) % 16;
    }
  }
}

void PartitionRoot::ShrinkEmptySlotSpansRing(size_t limit) {
  int16_t index = global_empty_slot_span_ring_index;
  int16_t starting_index = index;
  while (empty_slot_spans_dirty_bytes > limit) {
    internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>* slot_span =
        global_empty_slot_span_ring[index];
    // The ring is not always full, may be nullptr.
    if (slot_span) {
      slot_span->ToWritable(this)->DecommitIfPossible(this);
      // DecommitIfPossible() should set the buffer to null.
      PA_DCHECK(!global_empty_slot_span_ring[index]);
    }
    index += 1;
    // Walk through the entirety of possible slots, even though the last ones
    // are unused, if global_empty_slot_span_ring_size is smaller than
    // kMaxEmptySlotSpanRingSize. It's simpler, and does not cost anything,
    // since all the pointers are going to be nullptr.
    if (index == internal::kMaxEmptySlotSpanRingSize) {
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

void PartitionRoot::DumpStats(const char* partition_name,
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
  PartitionMemoryStats stats = {};

  stats.syscall_count = syscall_count.load(std::memory_order_relaxed);
  stats.syscall_total_time_ns =
      syscall_total_time_ns.load(std::memory_order_relaxed);

  // Collect data with the lock held, cannot allocate or call third-party code
  // below.
  {
    ::partition_alloc::internal::ScopedGuard guard{
        internal::PartitionRootLock(this)};
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
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
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
        internal::PartitionDumpBucketStats(&bucket_stats[i], this, bucket);
      }
      if (bucket_stats[i].is_valid) {
        stats.total_resident_bytes += bucket_stats[i].resident_bytes;
        stats.total_active_bytes += bucket_stats[i].active_bytes;
        stats.total_active_count += bucket_stats[i].active_count;
        stats.total_decommittable_bytes += bucket_stats[i].decommittable_bytes;
        stats.total_discardable_bytes += bucket_stats[i].discardable_bytes;
      }
    }

    for (const ReadOnlyDirectMapExtent* extent = direct_map_list;
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

    stats.has_thread_cache = settings.with_thread_cache;
    if (stats.has_thread_cache) {
      ThreadCacheRegistry::Instance().DumpStats(
          true, &stats.current_thread_cache_stats);
      ThreadCacheRegistry::Instance().DumpStats(false,
                                                &stats.all_thread_caches_stats);
    }

    stats.has_scheduler_loop_quarantine = settings.scheduler_loop_quarantine;
    if (stats.has_scheduler_loop_quarantine) {
      memset(
          reinterpret_cast<void*>(&stats.scheduler_loop_quarantine_stats_total),
          0, sizeof(LightweightQuarantineStats));
      scheduler_loop_quarantine_root.AccumulateStats(
          stats.scheduler_loop_quarantine_stats_total);
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
void PartitionRoot::DeleteForTesting(PartitionRoot* partition_root) {
  if (partition_root->settings.with_thread_cache) {
    ThreadCache::SwapForTesting(nullptr);
    partition_root->settings.with_thread_cache = false;
  }

  {
    ::partition_alloc::internal::ScopedGuard guard{
        internal::PartitionRootLock(partition_root)};
    partition_root->DestructForTesting();  // IN-TEST
  }

  delete partition_root;
}

void PartitionRoot::ResetForTesting(bool allow_leaks) {
  if (settings.with_thread_cache) {
    ThreadCache::SwapForTesting(nullptr);
    settings.with_thread_cache = false;
  }

  ::partition_alloc::internal::ScopedGuard guard{
      internal::PartitionRootLock(this)};

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  if (!allow_leaks) {
    unsigned num_allocated_slots = 0;
    for (Bucket& bucket : buckets) {
      if (bucket.active_slot_spans_head !=
          internal::SlotSpanMetadata<
              internal::MetadataKind::kReadOnly>::get_sentinel_slot_span()) {
        for (const internal::SlotSpanMetadata<
                 internal::MetadataKind::kReadOnly>* slot_span =
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
    bucket.active_slot_spans_head = internal::SlotSpanMetadata<
        internal::MetadataKind::kReadOnly>::get_sentinel_slot_span_non_const();
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
  for (auto*& entity : global_empty_slot_span_ring) {
    entity = nullptr;
  }

  global_empty_slot_span_ring_index = 0;
  global_empty_slot_span_ring_size = internal::kDefaultEmptySlotSpanRingSize;
  initialized = false;
}

void PartitionRoot::ResetBookkeepingForTesting() {
  ::partition_alloc::internal::ScopedGuard guard{
      internal::PartitionRootLock(this)};
  max_size_of_allocated_bytes = total_size_of_allocated_bytes;
  max_size_of_committed_pages.store(total_size_of_committed_pages);
}

void PartitionRoot::SetGlobalEmptySlotSpanRingIndexForTesting(int16_t index) {
  ::partition_alloc::internal::ScopedGuard guard{
      internal::PartitionRootLock(this)};
  global_empty_slot_span_ring_index = index;
}

ThreadCache* PartitionRoot::MaybeInitThreadCache() {
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

// static
void PartitionRoot::SetStraightenLargerSlotSpanFreeListsMode(
    StraightenLargerSlotSpanFreeListsMode new_value) {
  straighten_larger_slot_span_free_lists_ = new_value;
}

// static
void PartitionRoot::SetSortSmallerSlotSpanFreeListsEnabled(bool new_value) {
  sort_smaller_slot_span_free_lists_ = new_value;
}

// static
void PartitionRoot::SetSortActiveSlotSpansEnabled(bool new_value) {
  sort_active_slot_spans_ = new_value;
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
PA_NOINLINE void PartitionRoot::QuarantineForBrp(
    internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>* slot_span,
    void* object) {
  auto usable_size = GetSlotUsableSize(slot_span);
  auto hook = PartitionAllocHooks::GetQuarantineOverrideHook();
  if (hook) [[unlikely]] {
    hook(object, usable_size);
  } else {
    internal::SecureMemset(object, internal::kQuarantinedByte, usable_size);
  }
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// static
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
void PartitionRoot::EnableShadowMetadata(internal::PoolHandleMask mask) {
#if PA_BUILDFLAG(IS_LINUX)
  // TODO(crbug.com/40238514): implement ModuleCache() or something to
  // load required shared libraries in advance.
  // Since memfd_create() causes dlsym(), it is not possible to invoke
  // memfd_create() while PartitionRoot-s are locked.
  // So invoke memfd_create() here and invoke dysym() in advance.
  // This is required to enable ShadowMetadata on utility processes.
  { close(memfd_create("module_cache", MFD_CLOEXEC)); }
#endif

  internal::ScopedGuard guard(g_root_enumerator_lock);
  // Must lock all PartitionRoot-s and ThreadCache.
  internal::PartitionRootEnumerator::Instance().Enumerate(
      LockRoot, false,
      internal::PartitionRootEnumerator::EnumerateOrder::kNormal);
  {
    internal::ScopedGuard thread_cache_guard(ThreadCacheRegistry::GetLock());
    internal::PartitionAddressSpace::InitShadowMetadata(mask);
    internal::PartitionRootEnumerator::Instance().Enumerate(
        MakeSuperPageExtentEntriesShared, mask,
        internal::PartitionRootEnumerator::EnumerateOrder::kNormal);
  }
  internal::PartitionRootEnumerator::Instance().Enumerate(
      UnlockOrReinitRoot, false,
      internal::PartitionRootEnumerator::EnumerateOrder::kReverse);
}
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

// Explicitly define common template instantiations to reduce compile time.
#define EXPORT_TEMPLATE \
  template PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
EXPORT_TEMPLATE void* PartitionRoot::Alloc<AllocFlags::kNone>(size_t,
                                                              const char*);
EXPORT_TEMPLATE void* PartitionRoot::Alloc<AllocFlags::kReturnNull>(
    size_t,
    const char*);
EXPORT_TEMPLATE void*
PartitionRoot::Realloc<AllocFlags::kNone, FreeFlags::kNone>(void*,
                                                            size_t,
                                                            const char*);
EXPORT_TEMPLATE void*
PartitionRoot::Realloc<AllocFlags::kReturnNull, FreeFlags::kNone>(void*,
                                                                  size_t,
                                                                  const char*);
EXPORT_TEMPLATE void* PartitionRoot::AlignedAlloc<AllocFlags::kNone>(size_t,
                                                                     size_t);
#undef EXPORT_TEMPLATE

// TODO(crbug.com/40940915) Stop ignoring the -Winvalid-offsetof warning.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif
static_assert(offsetof(PartitionRoot, sentinel_bucket) ==
                  offsetof(PartitionRoot, buckets) +
                      internal::kNumBuckets * sizeof(PartitionRoot::Bucket),
              "sentinel_bucket must be just after the regular buckets.");

static_assert(
    offsetof(PartitionRoot, lock_) >= internal::kPartitionCachelineSize,
    "The lock should not be on the same cacheline as the read-mostly flags");
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace partition_alloc
