// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdint.h>
#include <sys/mman.h>

#include "partition_alloc/build_config.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_buildflags.h"

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING) || \
    (defined(__ARM_FEATURE_BTI_DEFAULT) && (__ARM_FEATURE_BTI_DEFAULT == 1))
struct __ifunc_arg_t;

#include "partition_alloc/aarch64_support.h"

#define NEEDS_HANDLING_OF_HW_CAPABILITIES
#endif

// PA_PROT_BTI requests a page that supports BTI landing pads.
#define PA_PROT_BTI 0x10

// PA_PROT_MTE requests a page that's suitable for memory tagging.
#define PA_PROT_MTE 0x20

namespace partition_alloc::internal {
namespace {

int GetAccessFlags(PageAccessibilityConfiguration accessibility,
                   bool mte_enabled,
                   bool bti_enabled) {
  switch (accessibility.permissions) {
    case PageAccessibilityConfiguration::kRead:
      return PROT_READ;
    case PageAccessibilityConfiguration::kReadWriteTagged:
      return PROT_READ | PROT_WRITE | (mte_enabled ? PA_PROT_MTE : 0);
    case PageAccessibilityConfiguration::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageAccessibilityConfiguration::kReadExecuteProtected:
      return PROT_READ | PROT_EXEC | (bti_enabled ? PA_PROT_BTI : 0);
    case PageAccessibilityConfiguration::kReadExecute:
      return PROT_READ | PROT_EXEC;
    case PageAccessibilityConfiguration::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    case PageAccessibilityConfiguration::kReadWriteExecuteProtected:
      return PROT_READ | PROT_WRITE | PROT_EXEC |
             (bti_enabled ? PA_PROT_BTI : 0);
    case PageAccessibilityConfiguration::kInaccessible:
    case PageAccessibilityConfiguration::kInaccessibleWillJitLater:
      return PROT_NONE;
  }
}

template <bool MteEnabled, bool BtiEnabled>
int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  return GetAccessFlags(accessibility, MteEnabled, BtiEnabled);
}

}  // namespace

#if defined(NEEDS_HANDLING_OF_HW_CAPABILITIES)
using GetAccessFlagsInternalFn = int(PageAccessibilityConfiguration);

extern "C" GetAccessFlagsInternalFn* ResolveGetAccessFlags(
    uint64_t hwcap,
    struct __ifunc_arg_t* hw) {
  if (IsMteEnabled(hwcap, hw)) {
    if (IsBtiEnabled(hwcap, hw)) {
      return GetAccessFlags<true, true>;
    } else {
      return GetAccessFlags<true, false>;
    }
  } else {
    if (IsBtiEnabled(hwcap, hw)) {
      return GetAccessFlags<false, true>;
    } else {
      return GetAccessFlags<false, false>;
    }
  }
}
#endif

// Resolve the implementation for GetAccessFlags using an iFunc.
int GetAccessFlags(PageAccessibilityConfiguration accessibility)
#if defined(NEEDS_HANDLING_OF_HW_CAPABILITIES)
    __attribute__((ifunc("ResolveGetAccessFlags")));
#else
{
  return GetAccessFlags<false, false>(accessibility);
}
#endif

}  // namespace partition_alloc::internal
