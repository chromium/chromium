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
ThreadCacheRegistry& ThreadCacheRegistry::Instance() {
  static NoDestructor<ThreadCacheRegistry> instance;
  return *instance.get();
}

ThreadCacheRegistry::ThreadCacheRegistry() = default;

void ThreadCacheRegistry::RegisterThreadCache(ThreadCache* cache) {
  AutoLock scoped_locker(GetLock());
  cache->next_ = nullptr;
  cache->prev_ = nullptr;

  ThreadCache* previous_head = list_head_;
  list_head_ = cache;
  cache->next_ = previous_head;
  if (previous_head)
    previous_head->prev_ = cache;
}

void ThreadCacheRegistry::UnregisterThreadCache(ThreadCache* cache) {
  AutoLock scoped_locker(GetLock());
  if (cache->prev_)
    cache->prev_->next_ = cache->next_;
  if (cache->next_)
    cache->next_->prev_ = cache->prev_;
  if (cache == list_head_)
    list_head_ = cache->next_;
}

void ThreadCacheRegistry::DumpStats(bool my_thread_only,
                                    ThreadCacheStats* stats) {
  memset(reinterpret_cast<void*>(stats), 0, sizeof(ThreadCacheStats));

  AutoLock scoped_locker(GetLock());
  if (my_thread_only) {
    auto* tcache = ThreadCache::Get();
    if (!tcache)
      return;
    tcache->AccumulateStats(stats);
  } else {
    ThreadCache* tcache = list_head_;
    while (tcache) {
      // Racy, as other threads are still allocating. This is not an issue,
      // since we are only interested in statistics. However, this means that
      // count is not necessarily equal to hits + misses for the various types
      // of events.
      tcache->AccumulateStats(stats);
      tcache = tcache->next_;
    }
  }
}

void ThreadCacheRegistry::PurgeAll() {
  auto* current_thread_tcache = ThreadCache::Get();

  {
    AutoLock scoped_locker(GetLock());
    ThreadCache* tcache = list_head_;
    while (tcache) {
      // Cannot purge directly, need to ask the other thread to purge "at some
      // point".
      // Note that this will not work if the other thread is sleeping forever.
      // TODO(lizeb): Handle sleeping threads.
      if (tcache != current_thread_tcache)
        tcache->SetShouldPurge();
      tcache = tcache->next_;
    }
  }

  // May take a while, don't hold the lock while purging.
  if (current_thread_tcache)
    current_thread_tcache->Purge();
}

// static
void ThreadCache::Init(PartitionRoot<ThreadSafe>* root) {
  PA_CHECK(root->buckets[kBucketCount - 1].slot_size == kSizeThreshold);

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
  ThreadCache* tcache = new (buffer) ThreadCache(root);

  // This may allocate.
  PartitionTlsSet(g_thread_cache_key, tcache);

  return tcache;
}

ThreadCache::ThreadCache(PartitionRoot<ThreadSafe>* root)
    : buckets_(), stats_(), root_(root), next_(nullptr), prev_(nullptr) {
  ThreadCacheRegistry::Instance().RegisterThreadCache(this);
}

ThreadCache::~ThreadCache() {
  ThreadCacheRegistry::Instance().UnregisterThreadCache(this);
  Purge();
}

void ThreadCache::AccumulateStats(ThreadCacheStats* stats) const {
  stats->alloc_count += stats_.alloc_count;
  stats->alloc_hits += stats_.alloc_hits;
  stats->alloc_misses += stats_.alloc_misses;

  stats->alloc_miss_empty += stats_.alloc_miss_empty;
  stats->alloc_miss_too_large += stats_.alloc_miss_too_large;

  stats->cache_fill_count += stats_.cache_fill_count;
  stats->cache_fill_hits += stats_.cache_fill_hits;
  stats->cache_fill_misses += stats_.cache_fill_misses;
  stats->cache_fill_bucket_full += stats_.cache_fill_bucket_full;
  stats->cache_fill_too_large += stats_.cache_fill_too_large;

  for (size_t i = 0; i < kBucketCount; i++) {
    stats->bucket_total_memory +=
        buckets_[i].count * root_->buckets[i].slot_size;
  }
  stats->metadata_overhead += sizeof(*this);
}

void ThreadCache::SetShouldPurge() {
  // We don't need any synchronization, and don't really care if the purge is
  // carried out "right away", hence relaxed atomics.
  should_purge_.store(true, std::memory_order_relaxed);
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
  should_purge_.store(false, std::memory_order_relaxed);
}

}  // namespace internal

}  // namespace base
