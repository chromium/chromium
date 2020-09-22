// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <sys/types.h>
#include <atomic>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc.h"

namespace base {

namespace internal {

BASE_EXPORT PartitionTlsKey g_thread_cache_key;

namespace {
void DeleteThreadCache(void* tcache_ptr) {
  reinterpret_cast<ThreadCache*>(tcache_ptr)->~ThreadCache();
  PartitionRoot<ThreadSafe>::RawFreeStatic(tcache_ptr);
}

// Since |g_thread_cache_key| is shared, make sure that no more than one
// PartitionRoot can use it.
static std::atomic<bool> g_has_instance;

}  // namespace

// static
void ThreadCache::Init(PartitionRoot<ThreadSafe>* root) {
  bool ok = PartitionTlsCreate(&g_thread_cache_key, DeleteThreadCache);
  PA_CHECK(ok);

  // Make sure that only one PartitionRoot wants a thread cache.
  bool expected = false;
  if (!g_has_instance.compare_exchange_strong(expected, true,
                                              std::memory_order_seq_cst,
                                              std::memory_order_seq_cst)) {
    PA_CHECK(false)
        << "Only one PartitionRoot is allowed to have a thread cache";
  }
}

// static
ThreadCache* ThreadCache::Create(PartitionRoot<internal::ThreadSafe>* root) {
  PA_CHECK(root);

  // Placement new and RawAlloc() are used, as otherwise when this partition is
  // the malloc() implementation, the memory allocated for the new thread cache
  // would make this code reentrant.
  //
  // This also means that deallocation must use RawFreeStatic(), hence the
  // operator delete() implementation below.
  size_t allocated_size;
  bool already_zeroed;

  auto* bucket =
      root->buckets + PartitionRoot<internal::ThreadSafe>::SizeToBucketIndex(
                          sizeof(ThreadCache));
  void* buffer =
      root->RawAlloc(bucket, PartitionAllocZeroFill, sizeof(ThreadCache),
                     &allocated_size, &already_zeroed);
  ThreadCache* tcache = new (buffer) ThreadCache();

  // This may allocate.
  PartitionTlsSet(g_thread_cache_key, tcache);

  return tcache;
}

ThreadCache::~ThreadCache() {
  Purge();
}

void ThreadCache::Purge() {
  for (Bucket& bucket : buckets_) {
    size_t count = bucket.count;

    while (bucket.freelist_head) {
      auto* entry = bucket.freelist_head;
      bucket.freelist_head = EncodedPartitionFreelistEntry::Decode(entry->next);

      PartitionRoot<ThreadSafe>::RawFreeStatic(entry);
      count--;
    }
    CHECK_EQ(0u, count);
    bucket.count = 0;
  }
}

}  // namespace internal

}  // namespace base
