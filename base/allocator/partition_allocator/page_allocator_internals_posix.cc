// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"

#include <sys/mman.h>

// PA_PROT_BTI requests a page that supports BTI landing pads.
#define PA_PROT_BTI 0x10
// PA_PROT_MTE requests a page that's suitable for memory tagging.
#define PA_PROT_MTE 0x20

namespace partition_alloc::internal {

int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  switch (accessibility.permissions) {
    case PageAccessibilityConfiguration::kRead:
      return PROT_READ;
    case PageAccessibilityConfiguration::kReadWriteTagged:
#if defined(ARCH_CPU_ARM64)
      return PROT_READ | PROT_WRITE |
             (base::CPU::GetInstanceNoAllocation().has_mte() ? PA_PROT_MTE : 0);
#else
      [[fallthrough]];
#endif
    case PageAccessibilityConfiguration::kReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageAccessibilityConfiguration::kReadExecuteProtected:
      return PROT_READ | PROT_EXEC |
             (base::CPU::GetInstanceNoAllocation().has_bti() ? PA_PROT_BTI : 0);
    case PageAccessibilityConfiguration::kReadExecute:
      return PROT_READ | PROT_EXEC;
    case PageAccessibilityConfiguration::kReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    case PageAccessibilityConfiguration::kInaccessible:
    case PageAccessibilityConfiguration::kInaccessibleWillJitLater:
      return PROT_NONE;
  }
}

}  // namespace partition_alloc::internal
