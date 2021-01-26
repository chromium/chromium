// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_H_

// ABOUT
// The allocators are designed to be extremely fast, thanks to the following
// properties and design:
// - Just two single (reasonably predicatable) branches in the hot / fast path
//   for both allocating and (significantly) freeing.
// - A minimal number of operations in the hot / fast path, with the slow paths
//   in separate functions, leading to the possibility of inlining.
// - Each partition page (which is usually multiple physical pages) has a
//   metadata structure which allows fast mapping of free() address to an
//   underlying bucket.
// - Supports a lock-free API for fast performance in single-threaded cases.
// - The freelist for a given bucket is split across a number of partition
//   pages, enabling various simple tricks to try and minimize fragmentation.
// - Fine-grained bucket sizes leading to less waste and better packing.
//
// The following security properties could be investigated in the future:
// - Per-object bucketing (instead of per-size) is mostly available at the API,
// but not used yet.
// - No randomness of freelist entries or bucket position.
// - Better checking for wild pointers in free().
// - Better freelist masking function to guarantee fault on 32-bit.

#include <limits.h>
#include <string.h>
#include <memory>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc-inl.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/partition_stats.h"
#include "base/allocator/partition_allocator/pcscan.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/base_export.h"
#include "base/bits.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/notreached.h"
#include "base/partition_alloc_buildflags.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#include <stdlib.h>
#endif

#if defined(ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#endif  // defined(ADDRESS_SANITIZER)

namespace base {

BASE_EXPORT void PartitionAllocGlobalInit(OomFunction on_out_of_memory);
BASE_EXPORT void PartitionAllocGlobalUninitForTesting();

namespace internal {
template <bool thread_safe>
struct BASE_EXPORT PartitionAllocator {
  PartitionAllocator() = default;
  ~PartitionAllocator();

  void init(PartitionOptions);

  ALWAYS_INLINE PartitionRoot<thread_safe>* root() { return &partition_root_; }
  ALWAYS_INLINE const PartitionRoot<thread_safe>* root() const {
    return &partition_root_;
  }

 private:
  PartitionRoot<thread_safe> partition_root_;
};

}  // namespace internal

using PartitionAllocator = internal::PartitionAllocator<internal::ThreadSafe>;
using ThreadUnsafePartitionAllocator =
    internal::PartitionAllocator<internal::NotThreadSafe>;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_H_
