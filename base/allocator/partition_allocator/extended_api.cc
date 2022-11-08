// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/extended_api.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/partition_allocator/thread_cache.h"

namespace partition_alloc::internal {

#if defined(PA_THREAD_CACHE_SUPPORTED)

namespace {

void DisableThreadCacheForRootIfEnabled(ThreadSafePartitionRoot* root) {
  // Some platforms don't have a thread cache, or it could already have been
  // disabled.
  if (!root || !root->flags.with_thread_cache)
    return;

  ThreadCacheRegistry::Instance().PurgeAll();
  root->flags.with_thread_cache = false;
  // Doesn't destroy the thread cache object(s). For background threads, they
  // will be collected (and free cached memory) at thread destruction
  // time. For the main thread, we leak it.
}

void EnablePartitionAllocThreadCacheForRootIfDisabled(
    ThreadSafePartitionRoot* root) {
  if (!root)
    return;
  root->flags.with_thread_cache = true;
}

#if BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT)
void DisablePartitionAllocThreadCacheForProcess() {
  auto* regular_allocator =
      allocator_shim::internal::PartitionAllocMalloc::Allocator();
  auto* aligned_allocator =
      allocator_shim::internal::PartitionAllocMalloc::AlignedAllocator();
  DisableThreadCacheForRootIfEnabled(regular_allocator);
  if (aligned_allocator != regular_allocator)
    DisableThreadCacheForRootIfEnabled(aligned_allocator);
  DisableThreadCacheForRootIfEnabled(
      allocator_shim::internal::PartitionAllocMalloc::OriginalAllocator());
}
#endif  // defined(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT)

}  // namespace

#endif  // defined(PA_THREAD_CACHE_SUPPORTED)

void SwapOutProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if defined(PA_THREAD_CACHE_SUPPORTED)

#if BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT)
  DisablePartitionAllocThreadCacheForProcess();
#else
  PA_CHECK(!ThreadCache::IsValid(ThreadCache::Get()));
#endif  // BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT)

  ThreadCache::SwapForTesting(root);
  EnablePartitionAllocThreadCacheForRootIfDisabled(root);

#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

void SwapInProcessThreadCacheForTesting(ThreadSafePartitionRoot* root) {
#if defined(PA_THREAD_CACHE_SUPPORTED)

  // First, disable the test thread cache we have.
  DisableThreadCacheForRootIfEnabled(root);

#if BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT)
  auto* regular_allocator =
      allocator_shim::internal::PartitionAllocMalloc::Allocator();
  EnablePartitionAllocThreadCacheForRootIfDisabled(regular_allocator);

  ThreadCache::SwapForTesting(regular_allocator);
#else
  ThreadCache::SwapForTesting(nullptr);
#endif  // BUILDFLAG(ENABLE_PARTITION_ALLOC_AS_MALLOC_SUPPORT)

#endif  // defined(PA_THREAD_CACHE_SUPPORTED)
}

ThreadAllocStats GetAllocStatsForCurrentThread() {
  ThreadCache* thread_cache = ThreadCache::Get();
  if (ThreadCache::IsValid(thread_cache))
    return thread_cache->thread_alloc_stats();
  return {};
}

}  // namespace partition_alloc::internal
