// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/lightweight_quarantine_support.h"

namespace partition_alloc {
ScopedSchedulerLoopQuarantineExclusion::
    ScopedSchedulerLoopQuarantineExclusion() {
  ThreadCache* tcache = ThreadCache::Get();
  if (!ThreadCache::IsValid(tcache)) {
    return;
  }
  instance_.emplace(tcache->GetSchedulerLoopQuarantineBranch());
}
ScopedSchedulerLoopQuarantineExclusion::
    ~ScopedSchedulerLoopQuarantineExclusion() {}
}  // namespace partition_alloc
