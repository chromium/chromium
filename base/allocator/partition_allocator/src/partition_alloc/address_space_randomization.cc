// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/address_space_randomization.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/random.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace partition_alloc {

uintptr_t GetRandomPageBase() {
  uintptr_t random = static_cast<uintptr_t>(internal::RandomValue());

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  random <<= 32ULL;
  random |= static_cast<uintptr_t>(internal::RandomValue());

  // The ASLRMask() and ASLROffset() constants will be suitable for the
  // OS and build configuration.
  random &= internal::ASLRMask();
  random += internal::ASLROffset();
#else  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#if PA_BUILDFLAG(IS_WIN)
  // On win32 host systems the randomization plus huge alignment causes
  // excessive fragmentation. Plus most of these systems lack ASLR, so the
  // randomization isn't buying anything. In that case we just skip it.
  // TODO(palmer): Just dump the randomization when HE-ASLR is present.
  static BOOL is_wow64 = -1;
  if (is_wow64 == -1 && !IsWow64Process(GetCurrentProcess(), &is_wow64)) {
    is_wow64 = FALSE;
  }
  if (!is_wow64) {
    return 0;
  }
#endif  // PA_BUILDFLAG(IS_WIN)
  random &= internal::ASLRMask();
  random += internal::ASLROffset();
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

  PA_DCHECK(!(random & internal::PageAllocationGranularityOffsetMask()));
  return random;
}

}  // namespace partition_alloc
