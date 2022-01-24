// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/cpu.h"

#include <sys/mman.h>

// PROT_BTI requests a page that supports BTI landing pads.
#define PROT_BTI 0x10
// PROT_MTE requests a page that's suitable for memory tagging.
#define PROT_MTE 0x20

namespace base {

int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  switch (accessibility) {
    case PageRead:
      return PROT_READ;
    case PageReadWriteTagged:
#if defined(ARCH_CPU_ARM64)
      return PROT_READ | PROT_WRITE |
             (CPU::GetInstanceNoAllocation().has_mte() ? PROT_MTE : 0u);
#else
      FALLTHROUGH;
#endif
    case PageReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageReadExecuteProtected:
      return PROT_READ | PROT_EXEC |
             (CPU::GetInstanceNoAllocation().has_bti() ? PROT_BTI : 0u);
    case PageReadExecute:
      return PROT_READ | PROT_EXEC;
    case PageReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      PA_NOTREACHED();
      FALLTHROUGH;
    case PageInaccessible:
      return PROT_NONE;
  }
}

}  // namespace base
