// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_H_

#include <stdint.h>

#include <cstddef>

#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace base {

enum PageAccessibilityConfiguration {
  PageInaccessible,
  PageRead,
  PageReadWrite,
  // This flag is mapped to PageReadWrite on systems that
  // don't support MTE.
  PageReadWriteTagged,
  // This flag is mapped to PageReadExecute on systems
  // that don't support Arm's BTI.
  PageReadExecuteProtected,
  PageReadExecute,
  // This flag is deprecated and will go away soon.
  // TODO(bbudge) Remove this as soon as V8 doesn't need RWX pages.
  PageReadWriteExecute,
};

// Use for De/RecommitSystemPages API.
enum PageAccessibilityDisposition {
  // Enforces permission update (Decommit will set to PageInaccessible;
  // Recommit will set to whatever was requested, other than PageInaccessible).
  PageUpdatePermissions,
  // Will not update permissions, if the platform supports that (POSIX & Fuchsia
  // only).
  PageKeepPermissionsIfPossible,
};

// macOS supports tagged memory regions, to help in debugging. On Android,
// these tags are used to name anonymous mappings.
enum class PageTag {
  kFirst = 240,           // Minimum tag value.
  kBlinkGC = 252,         // Blink GC pages.
  kPartitionAlloc = 253,  // PartitionAlloc, no matter the partition.
  kChromium = 254,        // Chromium page.
  kV8 = 255,              // V8 heap pages.
  kLast = kV8             // Maximum tag value.
};

BASE_EXPORT uintptr_t NextAlignedWithOffset(uintptr_t ptr,
                                            uintptr_t alignment,
                                            uintptr_t requested_offset);

// Allocate one or more pages.
//
// The requested |address| is just a hint; the actual address returned may
// differ. The returned address will be aligned to |align_offset| modulo |align|
// bytes.
//
// |length|, |align| and |align_offset| are in bytes, and must be a multiple of
// |PageAllocationGranularity()|. |length| and |align| must be non-zero.
// |align_offset| must be less than |align|. |align| must be a power of two.
//
// If |address| is null, then a suitable and randomized address will be chosen
// automatically.
//
// |accessibility| controls the permission of the allocated pages.
// PageInaccessible means uncommitted.
//
// |page_tag| is used on some platforms to identify the source of the
// allocation. Use PageTag::kChromium as a catch-all category.
//
// This call will return null if the allocation cannot be satisfied.
BASE_EXPORT void* AllocPages(void* address,
                             size_t length,
                             size_t align,
                             PageAccessibilityConfiguration accessibility,
                             PageTag page_tag);
BASE_EXPORT void* AllocPagesWithAlignOffset(
    void* address,
    size_t length,
    size_t align,
    size_t align_offset,
    PageAccessibilityConfiguration page_accessibility,
    PageTag page_tag);

// Free one or more pages starting at |address| and continuing for |length|
// bytes.
//
// |address| and |length| must match a previous call to |AllocPages|. Therefore,
// |address| must be aligned to |PageAllocationGranularity()| bytes, and
// |length| must be a multiple of |PageAllocationGranularity()|.
BASE_EXPORT void FreePages(void* address, size_t length);

// Mark one or more system pages, starting at |address| with the given
// |page_accessibility|. |length| must be a multiple of |SystemPageSize()|
// bytes.
//
// Returns true if the permission change succeeded. In most cases you must
// |CHECK| the result.
BASE_EXPORT WARN_UNUSED_RESULT bool TrySetSystemPagesAccess(
    void* address,
    size_t length,
    PageAccessibilityConfiguration page_accessibility);

// Mark one or more system pages, starting at |address| with the given
// |page_accessibility|. |length| must be a multiple of |SystemPageSize()|
// bytes.
//
// Performs a CHECK that the operation succeeds.
BASE_EXPORT void SetSystemPagesAccess(
    void* address,
    size_t length,
    PageAccessibilityConfiguration page_accessibility);

// Decommit one or more system pages starting at |address| and continuing for
// |length| bytes. |address| and |length| must be aligned to a system page
// boundary.
//
// |accessibility_disposition| allows to specify whether the pages should be
// made inaccessible (PageUpdatePermissions), or left as is
// (PageKeepPermissionsIfPossible, POSIX & Fuchsia only). The latter should only
// be used as an optimization if you really know what you're doing.
// TODO(bartekn): Ideally, all callers should use PageUpdatePermissions,
// for better security, but that may lead to a perf regression. Tracked at
// http://crbug.com/766882.
//
// Decommitted means that physical resources (RAM or swap) backing the allocated
// virtual address range may be released back to the system, but the address
// space is still allocated to the process (possibly using up page table entries
// or other accounting resources). There is no guarantee that the pages are
// zeroed, see |DecommittedMemoryIsAlwaysZeroed()| for such a guarantee. Unless
// PageKeepPermissionsIfPossible disposition is used, any access to a
// decommitted region of memory is an error and will generate a fault.
//
// This operation is not atomic on all platforms.
//
// Note: "Committed memory" is a Windows Memory Subsystem concept that ensures
// processes will not fault when touching a committed memory region. There is
// no analogue in the POSIX & Fuchsia memory API where virtual memory pages are
// best-effort allocated resources on the first touch. If PageUpdatePermissions
// disposition is used, this API behaves in a platform-agnostic way by
// simulating the Windows "decommit" state by both discarding the region
// (allowing the OS to avoid swap operations) *and* changing the page
// protections so accesses fault.
//
// This API will crash if the operation cannot be performed.
BASE_EXPORT void DecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition);

// Whether decommitted memory is guaranteed to be zeroed when it is
// recommitted. Do not assume that this will not change over time.
constexpr BASE_EXPORT bool DecommittedMemoryIsAlwaysZeroed() {
#if defined(OS_APPLE)
  return false;
#else
  return true;
#endif
}

// Recommit one or more system pages, starting at |address| and continuing for
// |length| bytes with the given |page_accessibility| (must not be
// PageInaccsessible). |address| and |length| must be aligned to a system page
// boundary.
//
// |accessibility_disposition| allows to specify whether the page permissions
// should be set to |page_accessibility| (PageUpdatePermissions), or left as is
// (PageKeepPermissionsIfPossible, POSIX & Fuchsia only). The latter can only be
// used if the pages were previously accessible and decommitted with
// PageKeepPermissionsIfPossible. It is ok, however, to recommit with
// PageUpdatePermissions even if pages were decommitted with
// PageKeepPermissionsIfPossible (merely losing an optimization).
//
// This operation is not atomic on all platforms.
//
// This API will crash if the operation cannot be performed.
BASE_EXPORT void RecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityConfiguration page_accessibility,
    PageAccessibilityDisposition accessibility_disposition);

// Like RecommitSystemPages(), but returns false instead of crashing.
BASE_EXPORT bool TryRecommitSystemPages(
    void* address,
    size_t length,
    PageAccessibilityConfiguration page_accessibility,
    PageAccessibilityDisposition accessibility_disposition) WARN_UNUSED_RESULT;

// Discard one or more system pages starting at |address| and continuing for
// |length| bytes. |length| must be a multiple of |SystemPageSize()|.
//
// Discarding is a hint to the system that the page is no longer required. The
// hint may:
//   - Do nothing.
//   - Discard the page immediately, freeing up physical pages.
//   - Discard the page at some time in the future in response to memory
//   pressure.
//
// Only committed pages should be discarded. Discarding a page does not decommit
// it, and it is valid to discard an already-discarded page. A read or write to
// a discarded page will not fault.
//
// Reading from a discarded page may return the original page content, or a page
// full of zeroes.
//
// Writing to a discarded page is the only guaranteed way to tell the system
// that the page is required again. Once written to, the content of the page is
// guaranteed stable once more. After being written to, the page content may be
// based on the original page content, or a page of zeroes.
BASE_EXPORT void DiscardSystemPages(void* address, size_t length);

// Rounds up |address| to the next multiple of |SystemPageSize()|. Returns
// 0 for an |address| of 0.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE uintptr_t
RoundUpToSystemPage(uintptr_t address) {
  return (address + SystemPageOffsetMask()) & SystemPageBaseMask();
}

// Rounds down |address| to the previous multiple of |SystemPageSize()|. Returns
// 0 for an |address| of 0.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE uintptr_t
RoundDownToSystemPage(uintptr_t address) {
  return address & SystemPageBaseMask();
}

// Rounds up |address| to the next multiple of |PageAllocationGranularity()|.
// Returns 0 for an |address| of 0.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE uintptr_t
RoundUpToPageAllocationGranularity(uintptr_t address) {
  return (address + PageAllocationGranularityOffsetMask()) &
         PageAllocationGranularityBaseMask();
}

// Rounds down |address| to the previous multiple of
// |PageAllocationGranularity()|. Returns 0 for an |address| of 0.
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE uintptr_t
RoundDownToPageAllocationGranularity(uintptr_t address) {
  return address & PageAllocationGranularityBaseMask();
}

// Reserves (at least) |size| bytes of address space, aligned to
// |PageAllocationGranularity()|. This can be called early on to make it more
// likely that large allocations will succeed. Returns true if the reservation
// succeeded, false if the reservation failed or a reservation was already made.
BASE_EXPORT bool ReserveAddressSpace(size_t size);

// Releases any reserved address space. |AllocPages| calls this automatically on
// an allocation failure. External allocators may also call this on failure.
//
// Returns true when an existing reservation was released.
BASE_EXPORT bool ReleaseReservation();

// Returns true if there is currently an address space reservation.
BASE_EXPORT bool HasReservationForTesting();

// Returns |errno| (POSIX) or the result of |GetLastError| (Windows) when |mmap|
// (POSIX) or |VirtualAlloc| (Windows) fails.
BASE_EXPORT uint32_t GetAllocPageErrorCode();

// Returns the total amount of mapped pages from all clients of
// PageAllocator. These pages may or may not be committed. This is mostly useful
// to assess address space pressure.
BASE_EXPORT size_t GetTotalMappedSize();

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_H_
