// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"

#include <limits.h>

#include <atomic>

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/bits.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_WIN)
#include "base/allocator/partition_allocator/page_allocator_internals_win.h"
#elif defined(OS_POSIX)
#include "base/allocator/partition_allocator/page_allocator_internals_posix.h"
#elif defined(OS_FUCHSIA)
#include "base/allocator/partition_allocator/page_allocator_internals_fuchsia.h"
#else
#error Platform not supported.
#endif

namespace base {

namespace {

LazyInstance<Lock>::Leaky g_reserve_lock = LAZY_INSTANCE_INITIALIZER;

// We may reserve/release address space on different threads.
Lock& GetReserveLock() {
  return g_reserve_lock.Get();
}

std::atomic<size_t> g_total_mapped_address_space;

// We only support a single block of reserved address space.
void* s_reservation_address GUARDED_BY(GetReserveLock()) = nullptr;
size_t s_reservation_size GUARDED_BY(GetReserveLock()) = 0;

void* AllocPagesIncludingReserved(void* address,
                                  size_t length,
                                  PageAccessibilityConfiguration accessibility,
                                  PageTag page_tag) {
  void* ret = SystemAllocPages(address, length, accessibility, page_tag);
  if (ret == nullptr) {
    const bool cant_alloc_length = kHintIsAdvisory || address == nullptr;
    if (cant_alloc_length) {
      // The system cannot allocate |length| bytes. Release any reserved address
      // space and try once more.
      ReleaseReservation();
      ret = SystemAllocPages(address, length, accessibility, page_tag);
    }
  }
  return ret;
}

// Trims |base| to given |trim_length| and |alignment|.
//
// On failure, on Windows, this function returns nullptr and frees |base|.
void* TrimMapping(void* base,
                  size_t base_length,
                  size_t trim_length,
                  uintptr_t alignment,
                  PageAccessibilityConfiguration accessibility) {
  size_t pre_slack = reinterpret_cast<uintptr_t>(base) & (alignment - 1);
  if (pre_slack) {
    pre_slack = alignment - pre_slack;
  }
  size_t post_slack = base_length - pre_slack - trim_length;
  PA_DCHECK(base_length >= trim_length || pre_slack || post_slack);
  PA_DCHECK(pre_slack < base_length);
  PA_DCHECK(post_slack < base_length);
  return TrimMappingInternal(base, base_length, trim_length, accessibility,
                             pre_slack, post_slack);
}

}  // namespace

void* SystemAllocPages(void* hint,
                       size_t length,
                       PageAccessibilityConfiguration accessibility,
                       PageTag page_tag) {
  PA_DCHECK(!(length & PageAllocationGranularityOffsetMask()));
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(hint) &
              PageAllocationGranularityOffsetMask()));
  void* ptr = SystemAllocPagesInternal(hint, length, accessibility, page_tag);
  if (ptr)
    g_total_mapped_address_space.fetch_add(length, std::memory_order_relaxed);

  return ptr;
}

void* AllocPages(void* address,
                 size_t length,
                 size_t align,
                 PageAccessibilityConfiguration accessibility,
                 PageTag page_tag) {
  PA_DCHECK(length >= PageAllocationGranularity());
  PA_DCHECK(!(length & PageAllocationGranularityOffsetMask()));
  PA_DCHECK(align >= PageAllocationGranularity());
  // Alignment must be power of 2 for masking math to work.
  PA_DCHECK(base::bits::IsPowerOfTwo(align));
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(address) &
              PageAllocationGranularityOffsetMask()));
  uintptr_t align_offset_mask = align - 1;
  uintptr_t align_base_mask = ~align_offset_mask;
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(address) & align_offset_mask));

  // If the client passed null as the address, choose a good one.
  if (address == nullptr) {
    address = GetRandomPageBase();
    address = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) &
                                      align_base_mask);
  }

  // First try to force an exact-size, aligned allocation from our random base.
#if defined(ARCH_CPU_32_BITS)
  // On 32 bit systems, first try one random aligned address, and then try an
  // aligned address derived from the value of |ret|.
  constexpr int kExactSizeTries = 2;
#else
  // On 64 bit systems, try 3 random aligned addresses.
  constexpr int kExactSizeTries = 3;
#endif

  for (int i = 0; i < kExactSizeTries; ++i) {
    void* ret =
        AllocPagesIncludingReserved(address, length, accessibility, page_tag);
    if (ret != nullptr) {
      // If the alignment is to our liking, we're done.
      if (!(reinterpret_cast<uintptr_t>(ret) & align_offset_mask))
        return ret;
      // Free the memory and try again.
      FreePages(ret, length);
    } else {
      // |ret| is null; if this try was unhinted, we're OOM.
      if (kHintIsAdvisory || address == nullptr)
        return nullptr;
    }

#if defined(ARCH_CPU_32_BITS)
    // For small address spaces, try the first aligned address >= |ret|. Note
    // |ret| may be null, in which case |address| becomes null.
    address = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(ret) + align_offset_mask) &
        align_base_mask);
#else  // defined(ARCH_CPU_64_BITS)
    // Keep trying random addresses on systems that have a large address space.
    address = GetRandomPageBase();
    address = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address) &
                                      align_base_mask);
#endif
  }

  // Make a larger allocation so we can force alignment.
  size_t try_length = length + (align - PageAllocationGranularity());
  PA_CHECK(try_length >= length);
  void* ret;

  do {
    // Continue randomizing only on POSIX.
    address = kHintIsAdvisory ? GetRandomPageBase() : nullptr;
    ret = AllocPagesIncludingReserved(address, try_length, accessibility,
                                      page_tag);
    // The retries are for Windows, where a race can steal our mapping on
    // resize.
  } while (ret != nullptr && (ret = TrimMapping(ret, try_length, length, align,
                                                accessibility)) == nullptr);

  return ret;
}

void FreePages(void* address, size_t length) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(address) &
              PageAllocationGranularityOffsetMask()));
  PA_DCHECK(!(length & PageAllocationGranularityOffsetMask()));
  FreePagesInternal(address, length);
  PA_DCHECK(g_total_mapped_address_space.load(std::memory_order_relaxed) > 0);
  g_total_mapped_address_space.fetch_sub(length, std::memory_order_relaxed);
}

bool TrySetSystemPagesAccess(void* address,
                             size_t length,
                             PageAccessibilityConfiguration accessibility) {
  PA_DCHECK(!(length & SystemPageOffsetMask()));
  return TrySetSystemPagesAccessInternal(address, length, accessibility);
}

void SetSystemPagesAccess(void* address,
                          size_t length,
                          PageAccessibilityConfiguration accessibility) {
  PA_DCHECK(!(length & SystemPageOffsetMask()));
  SetSystemPagesAccessInternal(address, length, accessibility);
}

void DecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(address) & SystemPageOffsetMask()));
  PA_DCHECK(!(length & SystemPageOffsetMask()));
  DecommitSystemPagesInternal(address, length, accessibility_disposition);
}

void RecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(address) & SystemPageOffsetMask()));
  PA_DCHECK(!(length & SystemPageOffsetMask()));
  PA_DCHECK(accessibility != PageInaccessible);
  RecommitSystemPagesInternal(address, length, accessibility,
                              accessibility_disposition);
}

bool TryRecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // Duplicated because we want errors to be reported at a lower level in the
  // crashing case.
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(address) & SystemPageOffsetMask()));
  PA_DCHECK(!(length & SystemPageOffsetMask()));
  PA_DCHECK(accessibility != PageInaccessible);
  return TryRecommitSystemPagesInternal(address, length, accessibility,
                                        accessibility_disposition);
}

void DiscardSystemPages(void* address, size_t length) {
  PA_DCHECK(!(length & SystemPageOffsetMask()));
  DiscardSystemPagesInternal(address, length);
}

bool ReserveAddressSpace(size_t size) {
  // To avoid deadlock, call only SystemAllocPages.
  AutoLock guard(GetReserveLock());
  if (s_reservation_address == nullptr) {
    void* mem =
        SystemAllocPages(nullptr, size, PageInaccessible, PageTag::kChromium);
    if (mem != nullptr) {
      // We guarantee this alignment when reserving address space.
      PA_DCHECK(!(reinterpret_cast<uintptr_t>(mem) &
                  PageAllocationGranularityOffsetMask()));
      s_reservation_address = mem;
      s_reservation_size = size;
      return true;
    }
  }
  return false;
}

bool ReleaseReservation() {
  // To avoid deadlock, call only FreePages.
  AutoLock guard(GetReserveLock());
  if (!s_reservation_address)
    return false;

  FreePages(s_reservation_address, s_reservation_size);
  s_reservation_address = nullptr;
  s_reservation_size = 0;
  return true;
}

bool HasReservationForTesting() {
  AutoLock guard(GetReserveLock());
  return s_reservation_address != nullptr;
}

uint32_t GetAllocPageErrorCode() {
  return s_allocPageErrorCode;
}

size_t GetTotalMappedSize() {
  return g_total_mapped_address_space;
}

}  // namespace base
