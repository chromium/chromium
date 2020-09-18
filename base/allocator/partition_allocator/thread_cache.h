// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_

#include <cstdint>
#include <memory>

#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_cookie.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_tls.h"
#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/partition_alloc_buildflags.h"

namespace base {

namespace internal {

class ThreadCache;

extern BASE_EXPORT PartitionTlsKey g_thread_cache_key;

// Per-thread cache. *Not* threadsafe, must only be accessed from a single
// thread.
//
// In practice, this is easily enforced as long as only |instance| is
// manipulated, as it is a thread_local member. As such, any
// |ThreadCache::instance->*()| call will necessarily be done from a single
// thread.
class BASE_EXPORT ThreadCache {
 public:
  // Initializes the thread cache for |root|. May allocate, so should be called
  // with the thread cache disabled on the partition side, and without the
  // partition lock held.
  //
  // May only be called by a single PartitionRoot.
  static void Init(PartitionRoot<ThreadSafe>* root);
  static void Init(PartitionRoot<NotThreadSafe>* root) { IMMEDIATE_CRASH(); }

  static ThreadCache* Get() {
    return reinterpret_cast<ThreadCache*>(PartitionTlsGet(g_thread_cache_key));
  }
  // Create a new ThreadCache associated with |root|.
  // Must be called without the partition locked, as this may allocate.
  static ThreadCache* Create(PartitionRoot<ThreadSafe>* root);
  static ThreadCache* Create(PartitionRoot<NotThreadSafe>* root) {
    IMMEDIATE_CRASH();
  }

  ~ThreadCache();

  // Force placement new.
  void* operator new(size_t) = delete;
  void* operator new(size_t, void* buffer) { return buffer; }
  void operator delete(void* ptr) = delete;
  ThreadCache(const ThreadCache&) = delete;
  ThreadCache(const ThreadCache&&) = delete;
  ThreadCache& operator=(const ThreadCache&) = delete;

  // Tries to put a memory block at |address| into the cache.
  // The block comes from the bucket at index |bucket_index| from the partition
  // this cache is for.
  //
  // Returns true if the memory was put in the cache, and false otherwise. This
  // can happen either because the cache is full or the allocation was too
  // large.
  ALWAYS_INLINE bool MaybePutInCache(void* address, size_t bucket_index);

  // Tries to allocate memory from the cache.
  // Returns nullptr for failure.
  //
  // Has the same behavior as RawAlloc(), that is: no cookie nor tag handling.
  ALWAYS_INLINE void* GetFromCache(size_t bucket_index);

  // Empties the cache.
  // The Partition lock must *not* be held when calling this.
  void Purge();

  size_t bucket_count_for_testing(size_t index) const {
    return buckets_[index].count;
  }

 private:
  ThreadCache() = default;

  struct Bucket {
    size_t count;
    PartitionFreelistEntry* freelist_head;
  };

  // TODO(lizeb): Optimize the threshold, and define it as an allocation size
  // rather than a bucket index.
  static constexpr size_t kBucketCount = 40;
  static_assert(
      kBucketCount < kNumBuckets,
      "Cannot have more cached buckets than what the allocator supports");
  // TODO(lizeb): Tune this constant, and adapt it to the bucket size /
  // allocation patterns.
  static constexpr size_t kMaxCountPerBucket = 100;

  Bucket buckets_[kBucketCount];

  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, LargeAllocationsAreNotCached);
  FRIEND_TEST_ALL_PREFIXES(ThreadCacheTest, MultipleThreadCaches);
};

ALWAYS_INLINE bool ThreadCache::MaybePutInCache(void* address,
                                                size_t bucket_index) {
  if (bucket_index >= kBucketCount)
    return false;

  auto& bucket = buckets_[bucket_index];

  if (bucket.count >= kMaxCountPerBucket)
    return false;

  PA_DCHECK(bucket.count != 0 || bucket.freelist_head == nullptr);

  auto* entry = reinterpret_cast<PartitionFreelistEntry*>(address);
  entry->next = PartitionFreelistEntry::Encode(bucket.freelist_head);
  bucket.freelist_head = entry;
  bucket.count++;
  return true;
}

ALWAYS_INLINE void* ThreadCache::GetFromCache(size_t bucket_index) {
  // Only handle "small" allocations.
  if (bucket_index >= kBucketCount)
    return nullptr;

  auto& bucket = buckets_[bucket_index];
  auto* result = bucket.freelist_head;
  if (!result) {
    PA_DCHECK(bucket.count == 0);
    return nullptr;
  }
  PA_DCHECK(bucket.count != 0);
  auto* next = EncodedPartitionFreelistEntry::Decode(result->next);
  PA_DCHECK(result != next);
  bucket.count--;
  PA_DCHECK(bucket.count != 0 || !next);
  bucket.freelist_head = next;
  return result;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_THREAD_CACHE_H_
