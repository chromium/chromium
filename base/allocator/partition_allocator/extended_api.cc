// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/extended_api.h"

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/thread_cache.h"

namespace base {

namespace internal {
void DisableThreadCacheForRootIfEnabled(ThreadSafePartitionRoot* root) {
  // Some platforms don't have a thread cache, or it could already have been
  // disabled.
  if (!root || !root->with_thread_cache)
    return;

  internal::ThreadCacheRegistry::Instance().PurgeAll();
  root->with_thread_cache = false;
  // Doesn't destroy the thread cache object(s). For background threads, they
  // will be collected (and free cached memory) at thread destruction
  // time. For the main thread, we leak it.
}

void EnablePartitionAllocThreadCacheForRootIfDisabled(
    ThreadSafePartitionRoot* root) {
  if (!root)
    return;
  root->with_thread_cache = true;
}

}  // namespace internal

void SwapOutProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
  DisablePartitionAllocThreadCacheForProcess();
  internal::ThreadCache::SwapForTesting(root);
  internal::EnablePartitionAllocThreadCacheForRootIfDisabled(root);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)
}

void SwapInProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
  // First, disable the test thread cache we have.
  internal::DisableThreadCacheForRootIfEnabled(root);

  auto* regular_allocator = base::internal::PartitionAllocMalloc::Allocator();
  internal::EnablePartitionAllocThreadCacheForRootIfDisabled(regular_allocator);

  internal::ThreadCache::SwapForTesting(regular_allocator);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)
}

void DisablePartitionAllocThreadCacheForProcess() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto* regular_allocator = base::internal::PartitionAllocMalloc::Allocator();
  auto* aligned_allocator =
      base::internal::PartitionAllocMalloc::AlignedAllocator();
  internal::DisableThreadCacheForRootIfEnabled(regular_allocator);
  if (aligned_allocator != regular_allocator)
    internal::DisableThreadCacheForRootIfEnabled(aligned_allocator);
  internal::DisableThreadCacheForRootIfEnabled(
      base::internal::PartitionAllocMalloc::OriginalAllocator());
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace base
