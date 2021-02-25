// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/thread_cache.h"

#include <sys/types.h>
#include <algorithm>
#include <atomic>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"

namespace base {

namespace internal {

namespace {

ThreadCacheRegistry g_instance;

}

BASE_EXPORT PartitionTlsKey g_thread_cache_key;

namespace {
// Since |g_thread_cache_key| is shared, make sure that no more than one
// PartitionRoot can use it.
static std::atomic<PartitionRoot<ThreadSafe>*> g_thread_cache_root;

#if defined(OS_WIN)
void OnDllProcessDetach() {
  // Very late allocations do occur (see crbug.com/1159411#c7 for instance),
  // including during CRT teardown. This is problematic for the thread cache
  // which relies on the CRT for TLS access for instance. This cannot be
  // mitigated inside the thread cache (since getting to it requires querying
  // TLS), but the PartitionRoot associated wih the thread cache can be made to
  // not use the thread cache anymore.
  g_thread_cache_root.load(std::memory_order_relaxed)->with_thread_cache =
      false;
}
#endif

static bool g_thread_cache_key_created = false;
}  // namespace

constexpr base::TimeDelta ThreadCacheRegistry::kMinPurgeInterval;
constexpr base::TimeDelta ThreadCacheRegistry::kMaxPurgeInterval;
constexpr base::TimeDelta ThreadCacheRegistry::kDefaultPurgeInterval;

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
  ThreadCache::EnsureThreadSpecificDataInitialized();
  memset(reinterpret_cast<void*>(stats), 0, sizeof(ThreadCacheStats));

  PartitionAutoLock scoped_locker(GetLock());
  if (my_thread_only) {
    auto* tcache = ThreadCache::Get();
    if (!ThreadCache::IsValid(tcache))
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
      PA_DCHECK(ThreadCache::IsValid(tcache));
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
  if (ThreadCache::IsValid(current_thread_tcache))
    current_thread_tcache->Purge();
}

void ThreadCacheRegistry::ForcePurgeAllThreadAfterForkUnsafe() {
  PartitionAutoLock scoped_locker(GetLock());
  ThreadCache* tcache = list_head_;
  while (tcache) {
#if DCHECK_IS_ON()
    // Before fork(), locks are acquired in the parent process. This means that
    // a concurrent allocation in the parent which must be filled by the central
    // allocator (i.e. the thread cache bucket is empty) will block inside the
    // thread cache waiting for the lock to be released.
    //
    // In the child process, this allocation will never complete since this
    // thread will not be resumed. However, calling |Purge()| triggers the
    // reentrancy guard since the parent process thread was suspended from
    // within the thread cache.
    // Clear the guard to prevent this from crashing.
    tcache->is_in_thread_cache_ = false;
#endif
    tcache->Purge();
    tcache = tcache->next_;
  }
}

void ThreadCacheRegistry::StartPeriodicPurge() {
  ThreadCache::EnsureThreadSpecificDataInitialized();

  // Can be called several times, don't post multiple tasks.
  if (periodic_purge_running_)
    return;

  periodic_purge_running_ = true;
  PostDelayedPurgeTask();
}

void ThreadCacheRegistry::PostDelayedPurgeTask() {
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThreadCacheRegistry::PeriodicPurge,
                     base::Unretained(this)),
      purge_interval_);
}

void ThreadCacheRegistry::PeriodicPurge() {
  // To stop periodic purge for testing.
  if (!periodic_purge_running_)
    return;

  ThreadCache* tcache = ThreadCache::Get();
  // Can run when there is no thread cache, in which case there is nothing to
  // do, and the task should not be rescheduled. This would typically indicate a
  // case where the thread cache was never enabled, or got disabled.
  if (!ThreadCache::IsValid(tcache))
    return;

  uint64_t allocations = tcache->stats_.alloc_count;
  uint64_t allocations_since_last_purge =
      allocations - allocations_at_last_purge_;

  // Purge should not run when there is little activity in the process. We
  // assume that the main thread is a reasonable proxy for the process activity,
  // where the main thread is the current one.
  //
  // If there were not enough allocations since the last purge, back off. On the
  // other hand, if there were many allocations, make purge more frequent, but
  // always in a set frequency range.
  //
  // There is a potential drawback: a process that was idle for a long time and
  // suddenly becomes very actve will take some time to go back to regularly
  // scheduled purge with a small enough interval. This is the case for instance
  // of a renderer moving to foreground. To mitigate that, if the number of
  // allocations since the last purge was very large, make a greater leap to
  // faster purging.
  if (allocations_since_last_purge > 10 * kMinMainThreadAllocationsForPurging) {
    purge_interval_ = std::min(kDefaultPurgeInterval, purge_interval_ / 2);
  } else if (allocations_since_last_purge >
             2 * kMinMainThreadAllocationsForPurging) {
    purge_interval_ = std::max(kMinPurgeInterval, purge_interval_ / 2);
  } else if (allocations_since_last_purge <
             kMinMainThreadAllocationsForPurging) {
    purge_interval_ = std::min(kMaxPurgeInterval, purge_interval_ * 2);
  }

  PurgeAll();

  allocations_at_last_purge_ = allocations;
  PostDelayedPurgeTask();
}

void ThreadCacheRegistry::ResetForTesting() {
  allocations_at_last_purge_ = 0;
  purge_interval_ = kDefaultPurgeInterval;
  periodic_purge_running_ = false;
}

// static
void ThreadCache::EnsureThreadSpecificDataInitialized() {
  // Using the registry lock to protect from concurrent initialization without
  // adding a special-pupose lock.
  PartitionAutoLock scoped_locker(ThreadCacheRegistry::Instance().GetLock());
  if (g_thread_cache_key_created)
    return;

  bool ok = PartitionTlsCreate(&g_thread_cache_key, Delete);
  PA_CHECK(ok);
  g_thread_cache_key_created = true;
}

// static
void ThreadCache::Init(PartitionRoot<ThreadSafe>* root) {
  PA_CHECK(root->buckets[kBucketCount - 1].slot_size == kSizeThreshold);

  EnsureThreadSpecificDataInitialized();

  // Make sure that only one PartitionRoot wants a thread cache.
  PartitionRoot<ThreadSafe>* expected = nullptr;
  if (!g_thread_cache_root.compare_exchange_strong(expected, root,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst)) {
    PA_CHECK(false)
        << "Only one PartitionRoot is allowed to have a thread cache";
  }

#if defined(OS_WIN)
  PartitionTlsSetOnDllProcessDetach(OnDllProcessDetach);
#endif
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
  size_t usable_size;
  bool already_zeroed;

  auto* bucket =
      root->buckets + PartitionRoot<internal::ThreadSafe>::SizeToBucketIndex(
                          sizeof(ThreadCache));
  void* buffer =
      root->RawAlloc(bucket, PartitionAllocZeroFill, sizeof(ThreadCache),
                     &usable_size, &already_zeroed);
  ThreadCache* tcache = new (buffer) ThreadCache(root);

  // This may allocate.
  PartitionTlsSet(g_thread_cache_key, tcache);

  return tcache;
}

ThreadCache::ThreadCache(PartitionRoot<ThreadSafe>* root)
    : buckets_(),
      should_purge_(false),
      stats_(),
      root_(root),
      next_(nullptr),
      prev_(nullptr) {
  ThreadCacheRegistry::Instance().RegisterThreadCache(this);

  for (int index = 0; index < kBucketCount; index++) {
    const auto& root_bucket = root->buckets[index];
    Bucket* tcache_bucket = &buckets_[index];
    // Invalid bucket.
    if (!root_bucket.active_slot_spans_head) {
      // Explicitly set this, as size computations iterate over all buckets.
      tcache_bucket->limit = 0;
      tcache_bucket->count = 0;
      tcache_bucket->slot_size = 0;
      continue;
    }

    // Smaller allocations are more frequent, and more performance-sensitive.
    // Cache more small objects, and fewer larger ones, to save memory.
    size_t slot_size = root_bucket.slot_size;
    PA_CHECK(slot_size <= std::numeric_limits<uint16_t>::max());
    tcache_bucket->slot_size = static_cast<uint16_t>(slot_size);

    if (slot_size <= 128) {
      tcache_bucket->limit = kMaxCountPerBucket;
    } else if (slot_size <= 256) {
      tcache_bucket->limit = kMaxCountPerBucket / 2;
    } else {
      tcache_bucket->limit = kMaxCountPerBucket / 4;
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

#if defined(OS_WIN)
  // On Windows, allocations do occur during thread/process teardown, make sure
  // they don't resurrect the thread cache.
  //
  // TODO(lizeb): Investigate whether this is needed on POSIX as well.
  PartitionTlsSet(g_thread_cache_key, reinterpret_cast<void*>(kTombstone));
#endif
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
  INCREMENT_COUNTER(stats_.batch_fill_count);

  Bucket& bucket = buckets_[bucket_index];
  int count = bucket.limit / kBatchFillRatio;

  size_t usable_size;
  bool is_already_zeroed;

  PA_DCHECK(!root_->buckets[bucket_index].CanStoreRawSize());
  PA_DCHECK(!root_->buckets[bucket_index].is_direct_mapped());

  // Same as calling RawAlloc() |count| times, but acquires the lock only once.
  internal::ScopedGuard<internal::ThreadSafe> guard(root_->lock_);
  for (int i = 0; i < count; i++) {
    // Thread cache fill should not trigger expensive operations, to not grab
    // the lock for a long time needlessly, but also to not inflate memory
    // usage. Indeed, without PartitionAllocFastPathOrReturnNull, cache fill may
    // activate a new PartitionPage, or even a new SuperPage, which is clearly
    // not desirable.
    //
    // |raw_size| is set to the slot size, as we don't know it. However, it is
    // only used for direct-mapped allocations and single-slot ones anyway,
    // which are not handled here.
    void* ptr = root_->AllocFromBucket(
        &root_->buckets[bucket_index],
        PartitionAllocFastPathOrReturnNull | PartitionAllocReturnNull,
        root_->buckets[bucket_index].slot_size /* raw_size */, &usable_size,
        &is_already_zeroed);

    // Either the previous allocation would require a slow path allocation, or
    // the central allocator is out of memory. If the bucket was filled with
    // some objects, then the allocation will be handled normally. Otherwise,
    // this goes to the central allocator, which will service the allocation,
    // return nullptr or crash.
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

void ThreadCache::ResetForTesting() {
  stats_.alloc_count = 0;
  stats_.alloc_hits = 0;
  stats_.alloc_misses = 0;

  stats_.alloc_miss_empty = 0;
  stats_.alloc_miss_too_large = 0;

  stats_.cache_fill_count = 0;
  stats_.cache_fill_hits = 0;
  stats_.cache_fill_misses = 0;

  stats_.batch_fill_count = 0;

  stats_.bucket_total_memory = 0;
  stats_.metadata_overhead = 0;

  Purge();
  should_purge_.store(false, std::memory_order_relaxed);
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

  stats->batch_fill_count += stats_.batch_fill_count;

  for (const Bucket& bucket : buckets_) {
    stats->bucket_total_memory +=
        bucket.count * static_cast<size_t>(bucket.slot_size);
  }
  stats->metadata_overhead += sizeof(*this);
}

void ThreadCache::SetShouldPurge() {
  should_purge_.store(true, std::memory_order_relaxed);
}

void ThreadCache::Purge() {
  PA_REENTRANCY_GUARD(is_in_thread_cache_);
  PurgeInternal();
}

void ThreadCache::PurgeInternal() {
  should_purge_.store(false, std::memory_order_relaxed);
  for (auto& bucket : buckets_)
    ClearBucket(bucket, 0);
}

}  // namespace internal

}  // namespace base
