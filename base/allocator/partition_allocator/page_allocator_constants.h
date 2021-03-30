// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_APPLE)

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

#else

// When defined, page size constants are fixed at compile time. When not
// defined, they may vary at run time.
#define PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR 1

// Use this macro to declare a function as constexpr or not based on whether
// PAGE_ALLOCATOR_CONSTANTS_ARE_CONSTEXPR is defined.
#define PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR constexpr

#endif

namespace {

#if !defined(OS_APPLE)

constexpr ALWAYS_INLINE int PageAllocationGranularityShift() {
#if defined(OS_WIN) || defined(ARCH_CPU_PPC64)
  // Modern ppc64 systems support 4kB (shift = 12) and 64kB (shift = 16) page
  // sizes.  Since 64kB is the de facto standard on the platform and binaries
  // compiled for 64kB are likely to work on 4kB systems, 64kB is a good choice
  // here.
  return 16;  // 64kB
#elif defined(_MIPS_ARCH_LOONGSON)
  return 14;  // 16kB
#else
  return 12;  // 4kB
#endif
}

#endif

}  // namespace

namespace base {

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
PageAllocationGranularity() {
#if defined(OS_APPLE)
  return vm_page_size;
#else
  return 1 << PageAllocationGranularityShift();
#endif
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
PageAllocationGranularityOffsetMask() {
  return PageAllocationGranularity() - 1;
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
PageAllocationGranularityBaseMask() {
  return ~PageAllocationGranularityOffsetMask();
}

#if !defined(OS_APPLE)
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
SystemPageShift() {
#if defined(OS_WIN)
  return 12;  // 4096=1<<12
#else
  return PageAllocationGranularityShift();
#endif
}
#endif

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
SystemPageSize() {
#if !defined(OS_APPLE)
  return 1 << SystemPageShift();
#else
  return PageAllocationGranularity();
#endif
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
SystemPageOffsetMask() {
  return SystemPageSize() - 1;
}

PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
SystemPageBaseMask() {
  return ~SystemPageOffsetMask();
}

static constexpr size_t kPageMetadataShift = 5;  // 32 bytes per partition page.
static constexpr size_t kPageMetadataSize = 1 << kPageMetadataShift;

// See DecommitSystemPages(), this is not guaranteed to be synchronous on all
// platforms.
static constexpr bool kDecommittedPagesAreAlwaysZeroed =
#if defined(OS_APPLE)
    false;
#else
    true;
#endif

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PAGE_ALLOCATOR_CONSTANTS_H_
