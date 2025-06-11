// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"

#include <atomic>
#include <bit>
#include <limits>

#include "base/containers/contains.h"
#include "base/synchronization/lock.h"

namespace base {

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

std::vector<size_t> LockFreeAddressHashSet::GetBucketLengths() const {
  lock_->AssertAcquired();
  std::vector<size_t> lengths;
  lengths.reserve(buckets_.size());
  for (const std::atomic<Node*>& bucket : buckets_) {
    size_t length = 0;
    for (const Node* node = bucket.load(std::memory_order_relaxed);
         node != nullptr; node = node->next) {
      // Count all non-null keys, including kDeletedKey, since they will need to
      // be searched when iterating.
      for (const KeySlot& key_slot : GetKeySlots(node)) {
        if (key_slot.load(std::memory_order_relaxed) == nullptr) {
          break;
        }
        ++length;
      }
    }
    lengths.push_back(length);
  }
  return lengths;
}

}  // namespace base
