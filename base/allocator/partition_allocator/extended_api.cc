// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/extended_api.h"

#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/thread_cache.h"

namespace base {

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)

namespace {

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

void DisablePartitionAllocThreadCacheForProcess() {
  auto* regular_allocator = internal::PartitionAllocMalloc::Allocator();
  auto* aligned_allocator = internal::PartitionAllocMalloc::AlignedAllocator();
  DisableThreadCacheForRootIfEnabled(regular_allocator);
  if (aligned_allocator != regular_allocator)
    DisableThreadCacheForRootIfEnabled(aligned_allocator);
  DisableThreadCacheForRootIfEnabled(
      internal::PartitionAllocMalloc::OriginalAllocator());
}

}  // namespace

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)

void SwapOutProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
  DisablePartitionAllocThreadCacheForProcess();
  internal::ThreadCache::SwapForTesting(root);
  EnablePartitionAllocThreadCacheForRootIfDisabled(root);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)
}

void SwapInProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
  // First, disable the test thread cache we have.
  DisableThreadCacheForRootIfEnabled(root);

  auto* regular_allocator = internal::PartitionAllocMalloc::Allocator();
  EnablePartitionAllocThreadCacheForRootIfDisabled(regular_allocator);

  internal::ThreadCache::SwapForTesting(regular_allocator);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // defined(PA_THREAD_CACHE_SUPPORTED)
}

}  // namespace base
