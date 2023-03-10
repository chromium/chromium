// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_

#include <stddef.h>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_64_BITS)

#include <mach/vm_page_size.h>

// Although page allocator constants are not constexpr, they are run-time
// constant. Because the underlying variables they access, such as vm_page_size,
// are not marked const, the compiler normally has no way to know that they
// don’t change and must obtain their values whenever it can't prove that they
// haven't been modified, even if they had already been obtained previously.
// Attaching __attribute__((const)) to these declarations allows these redundant
// accesses to be omitted under optimization such as common subexpression
// elimination.
#define PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR __attribute__((const))

#elif BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)
// This should work for all POSIX (if needed), but currently all other
// supported OS/architecture combinations use either hard-coded values
// (such as x86) or have means to determine these values without needing
// atomics (such as macOS on arm64).

// Page allocator constants are run-time constant
#define PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR __attribute__((const))

#include <unistd.h>
#include <atomic>

namespace partition_alloc::internal {

// Holds the current page size and shift, where size = 1 << shift
// Use PageAllocationGranularity(), PageAllocationGranularityShift()
// to initialize and retrieve these values safely.
struct PageCharacteristics {
  std::atomic<size_t> size;
  std::atomic<size_t> shift;
};
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
extern PageCharacteristics page_characteristics;

}  // namespace partition_alloc::internal

#else

// When defined, page size constants are fixed at compile time. When not
// defined, they may vary at run time.
#define PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR 1

// Use this macro to declare a function as constexpr or not based on whether
// PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR is defined.
#define PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR constexpr

#endif

namespace partition_alloc::internal {

// Forward declaration, implementation below
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PageAllocationGranularity();

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PageAllocationGranularityShift() {
#if BUILDFLAG(IS_WIN) || defined(ARCH_CPU_PPC64)
  // Modern ppc64 systems support 4kB (shift = 12) and 64kB (shift = 16) page
  // sizes.  Since 64kB is the de facto standard on the platform and binaries
  // compiled for 64kB are likely to work on 4kB systems, 64kB is a good choice
  // here.
  return 16;  // 64kB
#elif defined(_MIPS_ARCH_LOONGSON) || defined(ARCH_CPU_LOONG64)
  return 14;  // 16kB
#elif BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_64_BITS)
  return static_cast<size_t>(vm_page_shift);
#elif BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)
  // arm64 supports 4kb (shift = 12), 16kb (shift = 14), and 64kb (shift = 16)
  // page sizes. Retrieve from or initialize cache.
  size_t shift = page_characteristics.shift.load(std::memory_order_relaxed);
  if (PA_UNLIKELY(shift == 0)) {
    shift = static_cast<size_t>(
        __builtin_ctz((unsigned int)PageAllocationGranularity()));
    page_characteristics.shift.store(shift, std::memory_order_relaxed);
  }
  return shift;
#else
  return 12;  // 4kB
#endif
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PageAllocationGranularity() {
#if BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_64_BITS)
  // This is literally equivalent to |1 << PageAllocationGranularityShift()|
  // below, but was separated out for IS_APPLE to avoid << on a non-constexpr.
  return vm_page_size;
#elif BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64)
  // arm64 supports 4kb, 16kb, and 64kb page sizes. Retrieve from or
  // initialize cache.
  size_t size = page_characteristics.size.load(std::memory_order_relaxed);
  if (PA_UNLIKELY(size == 0)) {
    size = static_cast<size_t>(getpagesize());
    page_characteristics.size.store(size, std::memory_order_relaxed);
  }
  return size;
#else
  return 1 << PageAllocationGranularityShift();
#endif
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PageAllocationGranularityOffsetMask() {
  return PageAllocationGranularity() - 1;
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PageAllocationGranularityBaseMask() {
  return ~PageAllocationGranularityOffsetMask();
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
SystemPageShift() {
  // On Windows allocation granularity is higher than the page size. This comes
  // into play when reserving address space range (allocation granularity),
  // compared to committing pages into memory (system page granularity).
#if BUILDFLAG(IS_WIN)
  return 12;  // 4096=1<<12
#else
  return PageAllocationGranularityShift();
#endif
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
SystemPageSize() {
#if (BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_64_BITS)) || \
    (BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_ARM64))
  // This is literally equivalent to |1 << SystemPageShift()| below, but was
  // separated out for 64-bit IS_APPLE and arm64 on Linux to avoid << on a
  // non-constexpr.
  return PageAllocationGranularity();
#else
  return 1 << SystemPageShift();
#endif
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
SystemPageOffsetMask() {
  return SystemPageSize() - 1;
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
SystemPageBaseMask() {
  return ~SystemPageOffsetMask();
}

constexpr size_t kPageMetadataShift = 5;  // 32 bytes per partition page.
constexpr size_t kPageMetadataSize = 1 << kPageMetadataShift;

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_
