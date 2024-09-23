// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/page_allocator.h"

#include <atomic>
#include <cstdint>

#include "partition_alloc/address_space_randomization.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/page_allocator_internal.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_lock.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if PA_BUILDFLAG(IS_WIN)
#include "partition_alloc/page_allocator_internals_win.h"
#elif PA_BUILDFLAG(IS_POSIX)
#include "partition_alloc/page_allocator_internals_posix.h"
#elif PA_BUILDFLAG(IS_FUCHSIA)
#include "partition_alloc/page_allocator_internals_fuchsia.h"
#else
#error Platform not supported.
#endif

namespace partition_alloc {

namespace {

internal::Lock g_reserve_lock;

// We may reserve/release address space on different threads.
internal::Lock& GetReserveLock() {
  return g_reserve_lock;
}

std::atomic<size_t> g_total_mapped_address_space;

// We only support a single block of reserved address space.
uintptr_t s_reservation_address PA_GUARDED_BY(GetReserveLock()) = 0;
size_t s_reservation_size PA_GUARDED_BY(GetReserveLock()) = 0;

uintptr_t AllocPagesIncludingReserved(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageTag page_tag,
    int file_descriptor_for_shared_alloc = -1) {
  uintptr_t ret =
      internal::SystemAllocPages(address, length, accessibility, page_tag,
                                 file_descriptor_for_shared_alloc);
  if (!ret) {
    const bool cant_alloc_length = internal::kHintIsAdvisory || !address;
    if (cant_alloc_length) {
      // The system cannot allocate |length| bytes. Release any reserved address
      // space and try once more.
      ReleaseReservation();
      ret = internal::SystemAllocPages(address, length, accessibility, page_tag,
                                       file_descriptor_for_shared_alloc);
    }
  }
  return ret;
}

// Trims memory at |base_address| to given |trim_length| and |alignment|.
//
// On failure, on Windows, this function returns 0 and frees memory at
// |base_address|.
uintptr_t TrimMapping(uintptr_t base_address,
                      size_t base_length,
                      size_t trim_length,
                      uintptr_t alignment,
                      uintptr_t alignment_offset,
                      PageAccessibilityConfiguration accessibility) {
  PA_DCHECK(base_length >= trim_length);
  PA_DCHECK(internal::base::bits::HasSingleBit(alignment));
  PA_DCHECK(alignment_offset < alignment);
  uintptr_t new_base =
      NextAlignedWithOffset(base_address, alignment, alignment_offset);
  PA_DCHECK(new_base >= base_address);
  size_t pre_slack = new_base - base_address;
  size_t post_slack = base_length - pre_slack - trim_length;
  PA_DCHECK(base_length == trim_length || pre_slack || post_slack);
  PA_DCHECK(pre_slack < base_length);
  PA_DCHECK(post_slack < base_length);
  return internal::TrimMappingInternal(base_address, base_length, trim_length,
                                       accessibility, pre_slack, post_slack);
}

}  // namespace

// Align |address| up to the closest, non-smaller address, that gives
// |requested_offset| remainder modulo |alignment|.
//
// Examples for alignment=1024 and requested_offset=64:
//   64 -> 64
//   65 -> 1088
//   1024 -> 1088
//   1088 -> 1088
//   1089 -> 2112
//   2048 -> 2112
uintptr_t NextAlignedWithOffset(uintptr_t address,
                                uintptr_t alignment,
                                uintptr_t requested_offset) {
  PA_DCHECK(internal::base::bits::HasSingleBit(alignment));
  PA_DCHECK(requested_offset < alignment);

  uintptr_t actual_offset = address & (alignment - 1);
  uintptr_t new_address;
  if (actual_offset <= requested_offset) {
    new_address = address + requested_offset - actual_offset;
  } else {
    new_address = address + alignment + requested_offset - actual_offset;
  }
  PA_DCHECK(new_address >= address);
  PA_DCHECK(new_address - address < alignment);
  PA_DCHECK(new_address % alignment == requested_offset);

  return new_address;
}

namespace internal {

uintptr_t SystemAllocPages(uintptr_t hint,
                           size_t length,
                           PageAccessibilityConfiguration accessibility,
                           PageTag page_tag,
                           int file_descriptor_for_shared_alloc) {
  PA_DCHECK(!(length & internal::PageAllocationGranularityOffsetMask()));
  PA_DCHECK(!(hint & internal::PageAllocationGranularityOffsetMask()));
  uintptr_t ret = internal::SystemAllocPagesInternal(
      hint, length, accessibility, page_tag, file_descriptor_for_shared_alloc);
  if (ret) {
    g_total_mapped_address_space.fetch_add(length, std::memory_order_relaxed);
  }

  return ret;
}

}  // namespace internal

uintptr_t AllocPages(size_t length,
                     size_t align,
                     PageAccessibilityConfiguration accessibility,
                     PageTag page_tag,
                     int file_descriptor_for_shared_alloc) {
  return AllocPagesWithAlignOffset(0, length, align, 0, accessibility, page_tag,
                                   file_descriptor_for_shared_alloc);
}
uintptr_t AllocPages(uintptr_t address,
                     size_t length,
                     size_t align,
                     PageAccessibilityConfiguration accessibility,
                     PageTag page_tag) {
  return AllocPagesWithAlignOffset(address, length, align, 0, accessibility,
                                   page_tag);
}
void* AllocPages(void* address,
                 size_t length,
                 size_t align,
                 PageAccessibilityConfiguration accessibility,
                 PageTag page_tag) {
  return reinterpret_cast<void*>(
      AllocPages(reinterpret_cast<uintptr_t>(address), length, align,
                 accessibility, page_tag));
}

uintptr_t AllocPagesWithAlignOffset(
    uintptr_t address,
    size_t length,
    size_t align,
    size_t align_offset,
    PageAccessibilityConfiguration accessibility,
    PageTag page_tag,
    int file_descriptor_for_shared_alloc) {
  PA_DCHECK(length >= internal::PageAllocationGranularity());
  PA_DCHECK(!(length & internal::PageAllocationGranularityOffsetMask()));
  PA_DCHECK(align >= internal::PageAllocationGranularity());
  // Alignment must be power of 2 for masking math to work.
  PA_DCHECK(internal::base::bits::HasSingleBit(align));
  PA_DCHECK(align_offset < align);
  PA_DCHECK(!(align_offset & internal::PageAllocationGranularityOffsetMask()));
  PA_DCHECK(!(address & internal::PageAllocationGranularityOffsetMask()));
  uintptr_t align_offset_mask = align - 1;
  uintptr_t align_base_mask = ~align_offset_mask;
  PA_DCHECK(!address || (address & align_offset_mask) == align_offset);

  // If the client passed null as the address, choose a good one.
  if (!address) {
    address = (GetRandomPageBase() & align_base_mask) + align_offset;
  }

  // First try to force an exact-size, aligned allocation from our random base.
#if PA_BUILDFLAG(PA_ARCH_CPU_32_BITS)
  // On 32 bit systems, first try one random aligned address, and then try an
  // aligned address derived from the value of |ret|.
  constexpr int kExactSizeTries = 2;
#else
  // On 64 bit systems, try 3 random aligned addresses.
  constexpr int kExactSizeTries = 3;
#endif

  for (int i = 0; i < kExactSizeTries; ++i) {
    uintptr_t ret =
        AllocPagesIncludingReserved(address, length, accessibility, page_tag,
                                    file_descriptor_for_shared_alloc);
    if (ret) {
      // If the alignment is to our liking, we're done.
      if ((ret & align_offset_mask) == align_offset) {
        return ret;
      }
      // Free the memory and try again.
      FreePages(ret, length);
    } else {
      // |ret| is null; if this try was unhinted, we're OOM.
      if (internal::kHintIsAdvisory || !address) {
        return 0;
      }
    }

#if PA_BUILDFLAG(PA_ARCH_CPU_32_BITS)
    // For small address spaces, try the first aligned address >= |ret|. Note
    // |ret| may be null, in which case |address| becomes null. If
    // |align_offset| is non-zero, this calculation may get us not the first,
    // but the next matching address.
    address = ((ret + align_offset_mask) & align_base_mask) + align_offset;
#else  // PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
    // Keep trying random addresses on systems that have a large address space.
    address = NextAlignedWithOffset(GetRandomPageBase(), align, align_offset);
#endif
  }

  // Make a larger allocation so we can force alignment.
  size_t try_length = length + (align - internal::PageAllocationGranularity());
  PA_CHECK(try_length >= length);
  uintptr_t ret;

  do {
    // Continue randomizing only on POSIX.
    address = internal::kHintIsAdvisory ? GetRandomPageBase() : 0;
    ret =
        AllocPagesIncludingReserved(address, try_length, accessibility,
                                    page_tag, file_descriptor_for_shared_alloc);
    // The retries are for Windows, where a race can steal our mapping on
    // resize.
  } while (ret && (ret = TrimMapping(ret, try_length, length, align,
                                     align_offset, accessibility)) == 0);

  return ret;
}

void FreePages(uintptr_t address, size_t length) {
  PA_DCHECK(!(address & internal::PageAllocationGranularityOffsetMask()));
  PA_DCHECK(!(length & internal::PageAllocationGranularityOffsetMask()));
  internal::FreePagesInternal(address, length);
  PA_DCHECK(g_total_mapped_address_space.load(std::memory_order_relaxed) > 0);
  g_total_mapped_address_space.fetch_sub(length, std::memory_order_relaxed);
}
void FreePages(void* address, size_t length) {
  FreePages(reinterpret_cast<uintptr_t>(address), length);
}

bool TrySetSystemPagesAccess(uintptr_t address,
                             size_t length,
                             PageAccessibilityConfiguration accessibility) {
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  return internal::TrySetSystemPagesAccessInternal(address, length,
                                                   accessibility);
}
bool TrySetSystemPagesAccess(void* address,
                             size_t length,
                             PageAccessibilityConfiguration accessibility) {
  return TrySetSystemPagesAccess(reinterpret_cast<uintptr_t>(address), length,
                                 accessibility);
}

void SetSystemPagesAccess(uintptr_t address,
                          size_t length,
                          PageAccessibilityConfiguration accessibility) {
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  internal::SetSystemPagesAccessInternal(address, length, accessibility);
}

void SetSystemPagesAccess(void* address,
                          size_t length,
                          PageAccessibilityConfiguration accessibility) {
  SetSystemPagesAccess(reinterpret_cast<uintptr_t>(address), length,
                       accessibility);
}

void DecommitSystemPages(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  PA_DCHECK(!(address & internal::SystemPageOffsetMask()));
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  internal::DecommitSystemPagesInternal(address, length,
                                        accessibility_disposition);
}
void DecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  DecommitSystemPages(reinterpret_cast<uintptr_t>(address), length,
                      accessibility_disposition);
}

bool DecommitAndZeroSystemPages(uintptr_t address,
                                size_t length,
                                PageTag page_tag) {
  PA_DCHECK(!(address & internal::SystemPageOffsetMask()));
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  return internal::DecommitAndZeroSystemPagesInternal(address, length,
                                                      page_tag);
}

bool DecommitAndZeroSystemPages(void* address,
                                size_t length,
                                PageTag page_tag) {
  return DecommitAndZeroSystemPages(reinterpret_cast<uintptr_t>(address),
                                    length, page_tag);
}

void RecommitSystemPages(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  PA_DCHECK(!(address & internal::SystemPageOffsetMask()));
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  PA_DCHECK(accessibility.permissions !=
            PageAccessibilityConfiguration::kInaccessible);
  internal::RecommitSystemPagesInternal(address, length, accessibility,
                                        accessibility_disposition);
}

bool TryRecommitSystemPages(
    uintptr_t address,
    size_t length,
    PageAccessibilityConfiguration accessibility,
    PageAccessibilityDisposition accessibility_disposition) {
  // Duplicated because we want errors to be reported at a lower level in the
  // crashing case.
  PA_DCHECK(!(address & internal::SystemPageOffsetMask()));
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  PA_DCHECK(accessibility.permissions !=
            PageAccessibilityConfiguration::kInaccessible);
  return internal::TryRecommitSystemPagesInternal(
      address, length, accessibility, accessibility_disposition);
}

void DiscardSystemPages(uintptr_t address, size_t length) {
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  internal::DiscardSystemPagesInternal(address, length);
}
void DiscardSystemPages(void* address, size_t length) {
  DiscardSystemPages(reinterpret_cast<uintptr_t>(address), length);
}

bool SealSystemPages(uintptr_t address, size_t length) {
  PA_DCHECK(!(length & internal::SystemPageOffsetMask()));
  return internal::SealSystemPagesInternal(address, length);
}
bool SealSystemPages(void* address, size_t length) {
  return SealSystemPages(reinterpret_cast<uintptr_t>(address), length);
}

bool ReserveAddressSpace(size_t size) {
  // To avoid deadlock, call only SystemAllocPages.
  internal::ScopedGuard guard(GetReserveLock());
  if (!s_reservation_address) {
    uintptr_t mem = internal::SystemAllocPages(
        0, size,
        PageAccessibilityConfiguration(
            PageAccessibilityConfiguration::kInaccessible),
        PageTag::kChromium);
    if (mem) {
      // We guarantee this alignment when reserving address space.
      PA_DCHECK(!(mem & internal::PageAllocationGranularityOffsetMask()));
      s_reservation_address = mem;
      s_reservation_size = size;
      return true;
    }
  }
  return false;
}

bool ReleaseReservation() {
  // To avoid deadlock, call only FreePages.
  internal::ScopedGuard guard(GetReserveLock());
  if (!s_reservation_address) {
    return false;
  }

  FreePages(s_reservation_address, s_reservation_size);
  s_reservation_address = 0;
  s_reservation_size = 0;
  return true;
}

bool HasReservationForTesting() {
  internal::ScopedGuard guard(GetReserveLock());
  return s_reservation_address;
}

uint32_t GetAllocPageErrorCode() {
  return internal::s_allocPageErrorCode;
}

size_t GetTotalMappedSize() {
  return g_total_mapped_address_space;
}

#if PA_BUILDFLAG(IS_WIN)
namespace {
bool g_retry_on_commit_failure = false;
}

void SetRetryOnCommitFailure(bool retry_on_commit_failure) {
  g_retry_on_commit_failure = retry_on_commit_failure;
}

bool GetRetryOnCommitFailure() {
  return g_retry_on_commit_failure;
}
#endif

}  // namespace partition_alloc
