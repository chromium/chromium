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

namespace {

ThreadCacheRegistry g_instance;

}

BASE_EXPORT PartitionTlsKey g_thread_cache_key;

namespace {
// Since |g_thread_cache_key| is shared, make sure that no more than one
// PartitionRoot can use it.
static std::atomic<bool> g_has_instance;

}  // namespace

// static
ThreadCacheRegistry& ThreadCacheRegistry::Instance() {
  return g_instance;
}

void ThreadCacheRegistry::RegisterThreadCache(ThreadCache* cache) {
  PartitionAutoLock scoped_locker(GetLock());
  cache->next_ = nullptr;
  cache->prev_ = nullptr;

  ThreadCache* previous_head = list_head_;
  list_head_ = cache;
  cache->next_ = previous_head;
  if (previous_head)
    previous_head->prev_ = cache;
}

void ThreadCacheRegistry::UnregisterThreadCache(ThreadCache* cache) {
  PartitionAutoLock scoped_locker(GetLock());
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

  PartitionAutoLock scoped_locker(GetLock());
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
    PartitionAutoLock scoped_locker(GetLock());
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

  bool ok = PartitionTlsCreate(&g_thread_cache_key, Delete);
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
  size_t utilized_slot_size;
  bool already_zeroed;

  auto* bucket =
      root->buckets + PartitionRoot<internal::ThreadSafe>::SizeToBucketIndex(
                          sizeof(ThreadCache));
  void* buffer =
      root->RawAlloc(bucket, PartitionAllocZeroFill, sizeof(ThreadCache),
                     &utilized_slot_size, &already_zeroed);
  ThreadCache* tcache = new (buffer) ThreadCache(root);

  // This may allocate.
  PartitionTlsSet(g_thread_cache_key, tcache);

  return tcache;
}

ThreadCache::ThreadCache(PartitionRoot<ThreadSafe>* root)
    : buckets_(), stats_(), root_(root), next_(nullptr), prev_(nullptr) {
  ThreadCacheRegistry::Instance().RegisterThreadCache(this);

  for (int index = 0; index < kBucketCount; index++) {
    const auto& root_bucket = root->buckets[index];
    // Invalid bucket.
    if (!root_bucket.active_slot_spans_head)
      continue;

    // Smaller allocations are more frequent, and more performance-sensitive.
    // Cache more small objects, and fewer larger ones, to save memory.
    size_t element_size = root_bucket.slot_size;
    if (element_size <= 128) {
      buckets_[index].limit = 128;
    } else if (element_size <= 256) {
      buckets_[index].limit = 64;
    } else {
      buckets_[index].limit = 32;
    }
  }
}

ThreadCache::~ThreadCache() {
  ThreadCacheRegistry::Instance().UnregisterThreadCache(this);
  Purge();
}

// static
void ThreadCache::Delete(void* tcache_ptr) {
  auto* tcache = reinterpret_cast<ThreadCache*>(tcache_ptr);
  auto* root = tcache->root_;
  reinterpret_cast<ThreadCache*>(tcache_ptr)->~ThreadCache();
  root->RawFree(tcache_ptr);
}

void ThreadCache::FillBucket(size_t bucket_index) {
  // Filling multiple elements from the central allocator at a time has several
  // advantages:
  // - Amortize lock acquisition
  // - Increase hit rate
  // - Can improve locality, as consecutive allocations from the central
  //   allocator will likely return close addresses, especially early on.
  //
  // However, do not take too many items, to prevent memory bloat.
  //
  // Cache filling / purging policy:
  // We aim at keeping the buckets neither empty nor full, while minimizing
  // requests to the central allocator.
  //
  // For each bucket, there is a |limit| of how many cached objects there are in
  // the bucket, so |count| < |limit| at all times.
  // - Clearing: limit -> limit / 2
  // - Filling: 0 -> limit / kBatchFillRatio
  //
  // These thresholds are somewhat arbitrary, with these considerations:
  // (1) Batched filling should not completely fill the bucket
  // (2) Batched clearing should not completely clear the bucket
  // (3) Batched filling should not be too eager
  //
  // If (1) and (2) do not hold, we risk oscillations of bucket filling /
  // clearing which would greatly increase calls to the central allocator. (3)
  // tries to keep memory usage low. So clearing half of the bucket, and filling
  // a quarter of it are sensible defaults.
  Bucket& bucket = buckets_[bucket_index];
  int count = bucket.limit / kBatchFillRatio;

  size_t utilized_slot_size;
  bool is_already_zeroed;

  // Same as calling RawAlloc() |count| times, but acquires the lock only once.
  internal::ScopedGuard<internal::ThreadSafe> guard(root_->lock_);
  for (int i = 0; i < count; i++) {
    // We allow the allocator to return nullptr, since filling the cache may
    // safely fail, and the proper flag will be handled by the central
    // allocator.
    //
    // |raw_size| is set to the slot size, as we don't know it. However, it is
    // only used for direct-mapped allocations and single-slot ones anyway,
    // which are not handled here.
    void* ptr = root_->AllocFromBucket(
        &root_->buckets[bucket_index], PartitionAllocReturnNull,
        root_->buckets[bucket_index].slot_size /* raw_size */,
        &utilized_slot_size, &is_already_zeroed);
    // Central allocator is out of memory.
    if (!ptr)
      break;

    PutInBucket(bucket, ptr);
  }
}

void ThreadCache::ClearBucket(ThreadCache::Bucket& bucket, size_t limit) {
  // Avoids acquiring the lock needlessly.
  if (!bucket.count)
    return;

  // Acquire the lock once for the bucket. Allocations from the same bucket are
  // likely to be hitting the same cache lines in the central allocator, and
  // lock acquisitions can be expensive.
  internal::ScopedGuard<internal::ThreadSafe> guard(root_->lock_);
  while (bucket.count > limit) {
    auto* entry = bucket.freelist_head;
    PA_DCHECK(entry);
    bucket.freelist_head = entry->GetNext();

    root_->RawFreeLocked(entry);
    bucket.count--;
  }
  PA_DCHECK(bucket.count == limit);
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
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  PurgeInternal();
}

void ThreadCache::PurgeInternal() {
  for (auto& bucket : buckets_)
    ClearBucket(bucket, 0);

  should_purge_.store(false, std::memory_order_relaxed);
}

}  // namespace internal

}  // namespace base
