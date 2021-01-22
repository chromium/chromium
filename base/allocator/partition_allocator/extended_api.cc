// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/extended_api.h"

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/thread_cache.h"

namespace base {

void DisablePartitionAllocThreadCacheForProcess() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto* root = base::internal::PartitionAllocMalloc::Allocator();
  // Some platforms don't have a thread cache, or it could already have been
  // disabled.
  if (root->with_thread_cache) {
    internal::ThreadCacheRegistry::Instance().PurgeAll();
    root->with_thread_cache = false;

    // Doesn't destroy the thread cache object(s). For background threads, they
    // will be collected (and free cached memory) at thread destruction
    // time. For the main thread, we leak it.
  }
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace base
