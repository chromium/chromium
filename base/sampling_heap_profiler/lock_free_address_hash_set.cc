// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"

#include <atomic>
#include <bit>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/sampling_heap_profiler/lock_free_bloom_filter.h"
#include "base/synchronization/lock.h"

namespace base {

BASE_FEATURE(kUseLockFreeBloomFilter, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

// See the probability table in lock_free_bloom_filter.h to estimate the
// optimal bits per key. It's a tradeoff between better performance for the most
// common table sizes and better performance at outliers.
//
// Field data shows that on most platforms the hash table has about 5-10 entries
// at the 50th percentile, 10-20 entries at the 75th percentile, and 40-100
// entries at the 99th percentile. That gives expected false positive rates of:
//
// 2 bits per key:  2.1% to  7.2% at the 50th
//                  7.2% to 21.6% at the 75th
//                 50.9% to 91.4% at the 99th
//
// 3 bits per key:  0.9% to  5.2% at the 50th
//                  5.2% to 22.5% at the 75th
//                 60.7% to 97.3% at the 99th
//
// 4 bits per key:  0.5% to  4.7% at the 50th
//                  4.7% to 25.0% at the 75th
//                 71.0% to 99.2% at the 99th
constexpr base::FeatureParam<size_t> kBitsPerKey{&kUseLockFreeBloomFilter,
                                                 "bits_per_key", 3};

// Returns the result of a chi-squared test showing how evenly keys are
// distributed. `bucket_key_counts` is the count of keys stored in each bucket.
double ChiSquared(const std::vector<size_t>& bucket_key_counts) {
  // Algorithm taken from
  // https://en.wikipedia.org/wiki/Hash_function#Testing_and_measurement:
  // "n is the number of keys, m is the number of buckets, and b[j] is the
  // number of items in bucket j."
  const size_t n = std::accumulate(bucket_key_counts.begin(),
                                   bucket_key_counts.end(), size_t{0});
  const size_t m = bucket_key_counts.size();
  DCHECK(m);

  const double numerator = std::accumulate(
      bucket_key_counts.begin(), bucket_key_counts.end(), 0.0,
      [](double sum, size_t b) { return sum + b * (b + 1) / 2.0; });
  const double denominator = (n / (2.0 * m)) * (n + 2 * m - 1);
  // `denominator` could be 0 if n == 0. An empty set has uniformity 1.0 by
  // definition (all buckets have 0 keys).
  return denominator ? (numerator / denominator) : 1.0;
}

}  // namespace

LockFreeAddressHashSet::LockFreeAddressHashSet(size_t buckets_count, Lock& lock)
    : lock_(lock), buckets_(buckets_count), bucket_mask_(buckets_count - 1) {
  DCHECK(std::has_single_bit(buckets_count));
  DCHECK_LE(bucket_mask_, std::numeric_limits<uint32_t>::max());
  if (base::FeatureList::IsEnabled(kUseLockFreeBloomFilter)) {
    const size_t bits_per_key = kBitsPerKey.Get();
    CHECK_GT(bits_per_key, 0u);
    filter_.emplace(bits_per_key);
  }
}

LockFreeAddressHashSet::~LockFreeAddressHashSet() {
  for (std::atomic<Node*>& bucket : buckets_) {
    Node* node = bucket.load(std::memory_order_relaxed);
    while (node) {
      Node* next = node->next;
      delete node;
      node = next;
    }
  }
}

void LockFreeAddressHashSet::Insert(void* key) {
  lock_->AssertAcquired();
  DCHECK_NE(key, nullptr);
  CHECK_NE(Contains(key), ContainsResult::kFound);

  // Also store the key in the bloom filter.
  //
  // Note that other threads may be calling Contains() from a free hook while
  // Insert() is called from an alloc hook. In a well-behaved program `key` can
  // never be looked up until after Insert() returns, because it's returned from
  // alloc and passed to free, which happens-after alloc. But a race can happen
  // if one thread passes a random value to free while another receives the same
  // value from alloc. (This could happen if the program double-frees a pointer,
  // and the allocator reissues the same memory location between the two free
  // calls.)
  //
  // In this case, with only the hash set, there's a true race condition in the
  // free hook: if it sees the state before Insert(), Contains(key) returns
  // kNotFound and the free is ignored. If it sees the state after Insert(),
  // Contains(key) returns kFound and the profiler records that the memory was
  // freed. The only problem is that the memory stats could become incorrect,
  // and freeing an invalid pointer is undefined behaviour that makes the memory
  // stats unreliable anyway.
  //
  // When the Bloom filter is used, Insert() and Contains() are no longer
  // atomic. They each access `filter_` and `buckets_` in separate atomic
  // operations. This is safe because the possible outcomes don't change:
  //
  // When thread T1 races to look up `key` while thread T2 inserts it, T1 calls
  // filter_.MaybeContains(key), then only if MaybeContains returned true,
  // searches `buckets_` with FindNode(key). Meanwhile in T2, `filter_` and
  // `buckets_` can be updated in either order due to code reordering, and T1
  // can run between the updates. The possible states observed by T1 are
  // summarized in the following table.
  //
  // MaybeContains | FindNode     | Causes
  // --------------+--------------+---------------------------------------------
  // False         | undefined    | T1 sees state before Insert() call (FindNode
  //               | (not called) |   would return null)
  //               |              | T1 sees update to `buckets_` but not
  //               |              |   `filter_` (FindNode would return non-null)
  // --------------+--------------+---------------------------------------------
  // True          | NULL         | T1 sees state before Insert() call, with a
  //               |              |   false positive in `filter_`
  //               |              | T1 sees update to `filter_` but not
  //               |              |   `buckets_`
  // --------------+--------------+---------------------------------------------
  // True          | non-NULL     | T1 sees state after Insert() call
  //               |              | T1 sees update to `buckets_` but not
  //               |              |   `filter_`, which had a false positive
  //
  // Each of the 3 states that could be seen by T1 due to unsynchronized updates
  // of `filter_` and `buckets_` could already be seen by T1 if the updates
  // were sequentially consistent.
  if (filter_) {
    filter_->Add(key);
  }

  ++size_;
  // Note: There's no need to use std::atomic_compare_exchange here,
  // as we do not support concurrent inserts, so values cannot change midair.
  std::atomic<Node*>& bucket = buckets_[Hash(key) & bucket_mask_];
  Node* node = bucket.load(std::memory_order_relaxed);
  // First iterate over the bucket nodes and try to reuse an empty one if found.
  for (; node != nullptr; node = node->next) {
    if (node->key.load(std::memory_order_relaxed) == nullptr) {
      node->key.store(key, std::memory_order_relaxed);
      return;
    }
  }
  // There are no empty nodes to reuse left in the bucket.
  // Create a new node first...
  Node* new_node = new Node(key, bucket.load(std::memory_order_relaxed));
  // ... and then publish the new chain.
  bucket.store(new_node, std::memory_order_release);
}

void LockFreeAddressHashSet::Copy(const LockFreeAddressHashSet& other) {
  lock_->AssertAcquired();
  DCHECK_EQ(0u, size());
  for (const std::atomic<Node*>& bucket : other.buckets_) {
    for (const Node* node = bucket.load(std::memory_order_relaxed); node;
         node = node->next) {
      void* key = node->key.load(std::memory_order_relaxed);
      if (key) {
        Insert(key);
      }
    }
  }
}

LockFreeAddressHashSet::BucketStats LockFreeAddressHashSet::GetBucketStats()
    const {
  lock_->AssertAcquired();
  std::vector<size_t> lengths;
  lengths.reserve(buckets_.size());
  std::vector<size_t> key_counts;
  key_counts.reserve(buckets_.size());
  for (const std::atomic<Node*>& bucket : buckets_) {
    // Bucket length includes all nodes, including ones with null keys, since
    // they will need to be searched when iterating. Key count only includes
    // real keys.
    size_t length = 0;
    size_t key_count = 0;
    for (const Node* node = bucket.load(std::memory_order_relaxed);
         node != nullptr; node = node->next) {
      ++length;
      if (node->key.load(std::memory_order_relaxed)) {
        ++key_count;
      }
    }
    lengths.push_back(length);
    key_counts.push_back(key_count);
  }
  return BucketStats(std::move(lengths), ChiSquared(key_counts));
}

LockFreeAddressHashSet::BucketStats::BucketStats(std::vector<size_t> lengths,
                                                 double chi_squared)
    : lengths(std::move(lengths)), chi_squared(chi_squared) {}

LockFreeAddressHashSet::BucketStats::~BucketStats() = default;

LockFreeAddressHashSet::BucketStats::BucketStats(const BucketStats&) = default;

LockFreeAddressHashSet::BucketStats&
LockFreeAddressHashSet::BucketStats::operator=(const BucketStats&) = default;

}  // namespace base
