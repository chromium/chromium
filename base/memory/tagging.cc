// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/tagging.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "build/build_config.h"

#if defined(__ARM_FEATURE_MEMORY_TAGGING) && defined(ARCH_CPU_ARM64) && \
    (defined(OS_LINUX) || defined(OS_ANDROID))
#define HAS_MEMORY_TAGGING 1
#include <sys/auxv.h>
#include <sys/prctl.h>
#define HWCAP2_MTE (1 << 19)
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_GET_TAGGED_ADDR_CTRL 56
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)
#define PR_MTE_TCF_SHIFT 1
#define PR_MTE_TCF_NONE (0UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_SYNC (1UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_ASYNC (2UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_MASK (3UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TAG_SHIFT 3
#define PR_MTE_TAG_MASK (0xffffUL << PR_MTE_TAG_SHIFT)
#endif

namespace base {
namespace memory {

#if defined(HAS_MEMORY_TAGGING)
namespace {
void ChangeMemoryTaggingModeInternal(unsigned prctl_mask) {
  base::CPU cpu;
  if (cpu.has_mte()) {
    int status = prctl(PR_SET_TAGGED_ADDR_CTRL, prctl_mask, 0, 0, 0);
    if (status != 0) {
      LOG(FATAL) << "ChangeMemoryTaggingModeInternal: prctl failure";
    }
  }
}
}  // namespace
#endif  // defined(HAS_MEMORY_TAGGING)

void ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode m) {
#if defined(HAS_MEMORY_TAGGING)
  if (m == TagViolationReportingMode::kSynchronous) {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC |
                                    (0xfffe << PR_MTE_TAG_SHIFT));
  } else if (m == TagViolationReportingMode::kAsynchronous) {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC |
                                    (0xfffe << PR_MTE_TAG_SHIFT));
  } else {
    ChangeMemoryTaggingModeInternal(PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_NONE);
  }
#endif  // defined(HAS_MEMORY_TAGGING)
}

TagViolationReportingMode GetMemoryTaggingModeForCurrentThread() {
#if defined(HAS_MEMORY_TAGGING)
  base::CPU cpu;
  if (!cpu.has_mte()) {
    return TagViolationReportingMode::kUndefined;
  }
  int status = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
  if (status < 0) {
    LOG(FATAL) << "GetMemoryTaggingModeForCurrentThread: prctl failure";
  }
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_SYNC)) {
    return TagViolationReportingMode::kSynchronous;
  }
  if ((status & PR_TAGGED_ADDR_ENABLE) && (status & PR_MTE_TCF_ASYNC)) {
    return TagViolationReportingMode::kAsynchronous;
  }
#endif  // defined(HAS_MEMORY_TAGGING)
  return TagViolationReportingMode::kUndefined;
}

}  // namespace memory
}  // namespace base
