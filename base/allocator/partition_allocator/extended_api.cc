// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/extended_api.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "base/allocator/partition_allocator/thread_cache.h"

namespace partition_alloc::internal {

#if PA_CONFIG(THREAD_CACHE_SUPPORTED)

namespace {

void DisableThreadCacheForRootIfEnabled(PartitionRoot* root) {
  // Some platforms don't have a thread cache, or it could already have been
  // disabled.
  if (!root || !root->settings.with_thread_cache) {
    return;
  }

  ThreadCacheRegistry::Instance().PurgeAll();
  root->settings.with_thread_cache = false;
  // Doesn't destroy the thread cache object(s). For background threads, they
  // will be collected (and free cached memory) at thread destruction
  // time. For the main thread, we leak it.
}

void EnablePartitionAllocThreadCacheForRootIfDisabled(PartitionRoot* root) {
  if (!root) {
    return;
  }
  root->settings.with_thread_cache = true;
}

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void DisablePartitionAllocThreadCacheForProcess() {
  PA_CHECK(allocator_shim::internal::PartitionAllocMalloc::
               AllocatorConfigurationFinalized());
  auto* regular_allocator =
      allocator_shim::internal::PartitionAllocMalloc::Allocator();
  auto* aligned_allocator =
      allocator_shim::internal::PartitionAllocMalloc::AlignedAllocator();
  DisableThreadCacheForRootIfEnabled(regular_allocator);
  if (aligned_allocator != regular_allocator) {
    DisableThreadCacheForRootIfEnabled(aligned_allocator);
  }
  DisableThreadCacheForRootIfEnabled(
      allocator_shim::internal::PartitionAllocMalloc::OriginalAllocator());
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace

#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)

ThreadAllocStats GetAllocStatsForCurrentThread() {
  ThreadCache* thread_cache = ThreadCache::Get();
  if (ThreadCache::IsValid(thread_cache)) {
    return thread_cache->thread_alloc_stats();
  }
  return {};
}

#if PA_CONFIG(THREAD_CACHE_SUPPORTED)
ThreadCacheProcessScopeForTesting::ThreadCacheProcessScopeForTesting(
    PartitionRoot* root)
    : root_(root) {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto* regular_allocator =
      allocator_shim::internal::PartitionAllocMalloc::Allocator();
  regular_was_enabled_ =
      regular_allocator && regular_allocator->settings.with_thread_cache;

  if (root_ != regular_allocator) {
    // Another |root| is ThreadCache's PartitionRoot. Need to disable
    // thread cache for the process.
    DisablePartitionAllocThreadCacheForProcess();
    EnablePartitionAllocThreadCacheForRootIfDisabled(root_);
    // Replace ThreadCache's PartitionRoot.
    ThreadCache::SwapForTesting(root_);
  } else {
    if (!regular_was_enabled_) {
      EnablePartitionAllocThreadCacheForRootIfDisabled(root_);
      ThreadCache::SwapForTesting(root_);
    }
  }
#else
  PA_CHECK(!ThreadCache::IsValid(ThreadCache::Get()));
  EnablePartitionAllocThreadCacheForRootIfDisabled(root_);
  ThreadCache::SwapForTesting(root_);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  PA_CHECK(ThreadCache::Get());
}

ThreadCacheProcessScopeForTesting::~ThreadCacheProcessScopeForTesting() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  auto* regular_allocator =
      allocator_shim::internal::PartitionAllocMalloc::Allocator();
  bool regular_enabled =
      regular_allocator && regular_allocator->settings.with_thread_cache;

  if (regular_was_enabled_) {
    if (!regular_enabled) {
      // Need to re-enable ThreadCache for the process.
      EnablePartitionAllocThreadCacheForRootIfDisabled(regular_allocator);
      // In the case, |regular_allocator| must be ThreadCache's root.
      ThreadCache::SwapForTesting(regular_allocator);
    } else {
      // ThreadCache is enabled for the process, but we need to be
      // careful about ThreadCache's PartitionRoot. If it is different from
      // |regular_allocator|, we need to invoke SwapForTesting().
      if (regular_allocator != root_) {
        ThreadCache::SwapForTesting(regular_allocator);
      }
    }
  } else {
    // ThreadCache for all processes was disabled.
    DisableThreadCacheForRootIfEnabled(regular_allocator);
    ThreadCache::SwapForTesting(nullptr);
  }
#else
  // First, disable the test thread cache we have.
  DisableThreadCacheForRootIfEnabled(root_);

  ThreadCache::SwapForTesting(nullptr);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}
#endif  // PA_CONFIG(THREAD_CACHE_SUPPORTED)

}  // namespace partition_alloc::internal
