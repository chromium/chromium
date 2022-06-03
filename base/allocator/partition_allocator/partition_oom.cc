// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_oom.h"

#include "base/allocator/partition_allocator/oom.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "build/build_config.h"

namespace base {
namespace internal {

OomFunction g_oom_handling_function = nullptr;

NOINLINE void NOT_TAIL_CALLED PartitionExcessiveAllocationSize(size_t size) {
  NO_CODE_FOLDING();
  OOM_CRASH(size);
}

#if !defined(ARCH_CPU_64_BITS)
NOINLINE void NOT_TAIL_CALLED
PartitionOutOfMemoryWithLotsOfUncommitedPages(size_t size) {
  NO_CODE_FOLDING();
  OOM_CRASH(size);
}

[[noreturn]] NOINLINE void NOT_TAIL_CALLED
PartitionOutOfMemoryWithLargeVirtualSize(size_t virtual_size) {
  NO_CODE_FOLDING();
  OOM_CRASH(virtual_size);
}

#endif  // !defined(ARCH_CPU_64_BITS)

}  // namespace internal
}  // namespace base
