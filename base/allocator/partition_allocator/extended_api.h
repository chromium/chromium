// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_EXTENDED_API_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_EXTENDED_API_H_

#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/base_export.h"

namespace base {
// These two functions are unsafe to run if there are multiple threads running
// in the process.
//
// Disables the thread cache for the entire process, and replaces it with a
// thread cache for |root|.
BASE_EXPORT void SwapOutProcessThreadCacheForTesting(
    ThreadSafePartitionRoot* root);
// Disables the current thread cache, and replaces it with the default for the
// process.
BASE_EXPORT void SwapInProcessThreadCacheForTesting(
    ThreadSafePartitionRoot* root);

// Disables the thread cache for the entire process.
//
// Saves memory but slows down the allocator *significantly*. Only use for
// configurations that are very memory-constrained or performance-insensitive
// processes.
//
// Must preferably be called from the main thread, when no/few threads have
// been started.
//
// Otherwise, there are several things that can happen:
// 1. Another thread is currently temporarily disabling the thread cache, and
//    will re-enable it, negating this call's effect.
// 2. Other threads' caches cannot be purged from here, and would retain their
//    cached memory until thread destruction (where it is reclaimed).
//
// These are not correctness issues, at worst in the first case, memory is not
// saved, and in the second one, *some* of the memory is leaked.
BASE_EXPORT void DisablePartitionAllocThreadCacheForProcess();
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_EXTENDED_API_H_
