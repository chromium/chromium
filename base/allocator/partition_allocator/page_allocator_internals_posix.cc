// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/cpu.h"
#include "base/notreached.h"

#include <sys/mman.h>

// PROT_BTI requests a page that supports BTI landing pads.
#define PROT_BTI 0x10
// PROT_MTE requests a page that's suitable for memory tagging.
#define PROT_MTE 0x20

namespace base {

// Two helper functions to detect whether we can safely use PROT_BTI
// and PROT_MTE (static CPU triggers a -Wexit-time-destructors warning.)
static bool HasCPUBranchIdentification() {
#if defined(ARCH_CPU_ARM_FAMILY)
  CPU cpu = CPU::CreateNoAllocation();
  return cpu.has_bti();
#else
  return false;
#endif
}

static bool HasCPUMemoryTaggingExtension() {
#if defined(ARCH_CPU_ARM_FAMILY)
  CPU cpu = CPU::CreateNoAllocation();
  return cpu.has_mte();
#else
  return false;
#endif
}

int GetAccessFlags(PageAccessibilityConfiguration accessibility) {
  static const bool has_bti = HasCPUBranchIdentification();
  static const bool has_mte = HasCPUMemoryTaggingExtension();
  switch (accessibility) {
    case PageRead:
      return PROT_READ;
    case PageReadWrite:
      return PROT_READ | PROT_WRITE;
    case PageReadWriteTagged:
      return PROT_READ | PROT_WRITE | (has_mte ? PROT_MTE : 0u);
    case PageReadExecuteProtected:
      return PROT_READ | PROT_EXEC | (has_bti ? PROT_BTI : 0u);
    case PageReadExecute:
      return PROT_READ | PROT_EXEC;
    case PageReadWriteExecute:
      return PROT_READ | PROT_WRITE | PROT_EXEC;
    default:
      NOTREACHED();
      FALLTHROUGH;
    case PageInaccessible:
      return PROT_NONE;
  }
}

}  // namespace base
