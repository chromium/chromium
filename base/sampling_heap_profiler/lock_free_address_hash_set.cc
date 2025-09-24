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

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/synchronization/lock.h"

namespace base {

namespace {

// Returns the result of a chi-squared test showing how evenly keys are
// distributed. `bucket_key_counts` is the count of keys stored in each bucket.
double ChiSquared(const std::vector<size_t>& bucket_key_counts) {
  // Algorithm taken from
  // https://en.wikipedia.org/wiki/Hash_function#Testing_and_measurement:
  // "n is the number of keys, m is the number of buckets, and b[j] is the
  // number of items in bucket j."
  const size_t n =
      std::accumulate(bucket_key_counts.begin(), bucket_key_counts.end(), 0u);
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

void* const LockFreeAddressHashSet::kDeletedKey =
    reinterpret_cast<void*>(intptr_t{-1});

LockFreeAddressHashSet::LockFreeAddressHashSet(size_t buckets_count,
                                               Lock& lock,
                                               bool multi_key)
    : lock_(lock),
      buckets_(buckets_count),
      bucket_mask_(buckets_count - 1),
      multi_key_(multi_key) {
  DCHECK(std::has_single_bit(buckets_count));
  DCHECK_LE(bucket_mask_, std::numeric_limits<uint32_t>::max());
}

LockFreeAddressHashSet::~LockFreeAddressHashSet() {
  for (std::atomic<Node*>& bucket : buckets_) {
    Node* node = bucket.load(std::memory_order_relaxed);
    while (node) {
      Node* next = node->next;
      if (multi_key_) {
        delete reinterpret_cast<MultiKeyNode*>(node);
      } else {
        delete reinterpret_cast<SingleKeyNode*>(node);
      }
      node = next;
    }
  }
}

void LockFreeAddressHashSet::Insert(void* key) {
  lock_->AssertAcquired();
  DCHECK_NE(key, nullptr);
  DCHECK_NE(key, kDeletedKey);
  CHECK(!Contains(key));
  ++size_;
  // Note: There's no need to use std::atomic_compare_exchange here,
  // as we do not support concurrent inserts, so values cannot change midair.
  std::atomic<Node*>& bucket = buckets_[Hash(key) & bucket_mask_];
  Node* node = bucket.load(std::memory_order_relaxed);
  // First iterate over the bucket nodes and try to use an empty key slot.
  for (; node != nullptr; node = node->next) {
    for (KeySlot& key_slot : GetKeySlots(node)) {
      void* existing_key = key_slot.load(std::memory_order_relaxed);
      if (existing_key == nullptr || existing_key == kDeletedKey) {
        key_slot.store(key, std::memory_order_relaxed);
        return;
      }
    }
  }
  // There are no empty key slots to reuse left in the bucket.
  // Create a new node first...
  Node* new_node;
  if (multi_key_) {
    new_node = new MultiKeyNode(key, bucket.load(std::memory_order_relaxed));
  } else {
    new_node = new SingleKeyNode(key, bucket.load(std::memory_order_relaxed));
  }
  // ... and then publish the new chain.
  bucket.store(new_node, std::memory_order_release);
}

void LockFreeAddressHashSet::Copy(const LockFreeAddressHashSet& other) {
  lock_->AssertAcquired();
  DCHECK_EQ(0u, size());
  for (const std::atomic<Node*>& bucket : other.buckets_) {
    for (const Node* node = bucket.load(std::memory_order_relaxed); node;
         node = node->next) {
      for (const KeySlot& key_slot : other.GetKeySlots(node)) {
        void* key = key_slot.load(std::memory_order_relaxed);
        if (key != nullptr && key != kDeletedKey) {
          Insert(key);
        }
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
    // Bucket length includes all non-null values, including kDeletedKey, since
    // they will need to be searched when iterating. Key count only includes
    // real keys.
    size_t length = 0;
    size_t key_count = 0;
    for (const Node* node = bucket.load(std::memory_order_relaxed);
         node != nullptr; node = node->next) {
      for (const KeySlot& key_slot : GetKeySlots(node)) {
        void* key = key_slot.load(std::memory_order_relaxed);
        if (key == nullptr) {
          break;
        }
        ++length;
        if (key != kDeletedKey) {
          ++key_count;
        }
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
