// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/oom.h"

#include "base/allocator/partition_allocator/oom_callback.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/immediate_crash.h"
#include "base/process/memory.h"

namespace partition_alloc::internal {

// The crash is generated in a PA_NOINLINE function so that we can classify the
// crash as an OOM solely by analyzing the stack trace. It is tagged as
// PA_NOT_TAIL_CALLED to ensure that its parent function stays on the stack.
[[noreturn]] PA_NOINLINE void PA_NOT_TAIL_CALLED OnNoMemory(size_t size) {
  RunPartitionAllocOomCallback();
  base::TerminateBecauseOutOfMemory(size);
  PA_IMMEDIATE_CRASH();
}

}  // namespace partition_alloc::internal
