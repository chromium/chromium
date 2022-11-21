// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_space_randomization.h"

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/random.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>  // Must be in front of other Windows header files.

#include <versionhelpers.h>
#endif

namespace partition_alloc {

uintptr_t GetRandomPageBase() {
  uintptr_t random = static_cast<uintptr_t>(internal::RandomValue());

#if defined(PA_HAS_64_BITS_POINTERS)
  random <<= 32ULL;
  random |= static_cast<uintptr_t>(internal::RandomValue());

// The ASLRMask() and ASLROffset() constants will be suitable for the
// OS and build configuration.
#if BUILDFLAG(IS_WIN) && !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // Windows >= 8.1 has the full 47 bits. Use them where available.
  static bool windows_81 = false;
  static bool windows_81_initialized = false;
  if (!windows_81_initialized) {
    windows_81 = IsWindows8Point1OrGreater();
    windows_81_initialized = true;
  }
  if (!windows_81) {
    random &= internal::ASLRMaskBefore8_10();
  } else {
    random &= internal::ASLRMask();
  }
  random += internal::ASLROffset();
#else
  random &= internal::ASLRMask();
  random += internal::ASLROffset();
#endif  // BUILDFLAG(IS_WIN) && !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#else   // defined(PA_HAS_64_BITS_POINTERS)
#if BUILDFLAG(IS_WIN)
  // On win32 host systems the randomization plus huge alignment causes
  // excessive fragmentation. Plus most of these systems lack ASLR, so the
  // randomization isn't buying anything. In that case we just skip it.
  // TODO(palmer): Just dump the randomization when HE-ASLR is present.
  static BOOL is_wow64 = -1;
  if (is_wow64 == -1 && !IsWow64Process(GetCurrentProcess(), &is_wow64))
    is_wow64 = FALSE;
  if (!is_wow64)
    return 0;
#endif  // BUILDFLAG(IS_WIN)
  random &= internal::ASLRMask();
  random += internal::ASLROffset();
#endif  // defined(PA_HAS_64_BITS_POINTERS)

  PA_DCHECK(!(random & internal::PageAllocationGranularityOffsetMask()));
  return random;
}

}  // namespace partition_alloc
