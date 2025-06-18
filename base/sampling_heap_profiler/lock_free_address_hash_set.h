// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_ADDRESS_HASH_SET_H_
#define BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_ADDRESS_HASH_SET_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <new>
#include <vector>

#include "base/base_export.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace base {

// A hash set container that provides lock-free version of |Contains| operation.
// It does not support concurrent write operations |Insert| and |Remove|.
// All write operations if performed from multiple threads must be properly
// guarded with a lock.
// |Contains| method can be executed concurrently with other |Insert|, |Remove|,
// or |Contains| even over the same key.
// However, please note the result of concurrent execution of |Contains|
// with |Insert| or |Remove| over the same key is racy.
//
// The hash set never rehashes, so the number of buckets stays the same
// for the lifetime of the set.
//
// Internally the hashset is implemented as a vector of N buckets
// (N has to be a power of 2). Each bucket holds a single-linked list of
// nodes each containing keys.
//
// As an optimization, each node can optionally hold a fixed-length array of
// keys, so that in most cases all keys in the bucket share a cache line.
// Ideally only in extreme cases will a bucket hold so many keys that a second
// node in a different area of the heap must be allocated.
//
// It is not possible to really delete nodes from the list as there might
// be concurrent reads being executed over the node. The |Remove| operation
// just marks the node as empty by placing a sentinel into its key field.
// Consequent |Insert| operations may reuse empty nodes when possible.
//
// The structure of the hashset for N buckets is the following (assuming 2 keys
// per node):
//
// 0: {*}--> {[key1,key2],*}--> NULL
// 1: {*}--> NULL
// 2: {*}--> {[kDeletedKey,key3],*}--> {[key4,NULL],*}--> NULL
// ...
// N-1: {*}--> {[keyM,NULL],*}--> NULL
//
// In bucket 2, three keys were inserted. The third required a second node,
// containing the array [key4,NULL]. Then a key was removed, leaving a
// kDeletedKey in the first node that can be reused if needed.
class BASE_EXPORT LockFreeAddressHashSet {
 public:
  // Stats about the hash set's buckets, for metrics.
  struct BASE_EXPORT BucketStats {
    BucketStats(std::vector<size_t> lengths, double chi_squared);
    ~BucketStats();

    BucketStats(const BucketStats&);
    BucketStats& operator=(const BucketStats&);

    // Length of each bucket (ie. number of key slots that must be searched).
    std::vector<size_t> lengths;

    // Result of a chi-squared test that measures uniformity of bucket usage.
    double chi_squared = 0.0;
  };

  // Creates a hash set with `buckets_count` buckets. `lock` is a lock that
  // must be held by callers of |Insert|, |Remove| and |Copy|. |Contains| is
  // lock-free.
  LockFreeAddressHashSet(size_t buckets_count,
                         Lock& lock,
                         bool multi_key = false);

  ~LockFreeAddressHashSet();

  // Checks if the |key|, which must not be nullptr or kDeletedKey, is in the
  // set.
  // Can be executed concurrently with |Insert|, |Remove|, and |Contains|
  // operations.
  ALWAYS_INLINE bool Contains(void* key) const;

  // Removes the |key|, which must not be nullptr or kDeletedKey, from the set.
  // The key must be present in the set before the invocation.
  // Concurrent execution of |Insert|, |Remove|, or |Copy| is not supported.
  ALWAYS_INLINE void Remove(void* key);

  // Inserts the |key|, which must not be nullptr or kDeletedKey, into the set.
  // The key must not be present in the set before the invocation.
  // Concurrent execution of |Insert|, |Remove|, or |Copy| is not supported.
  void Insert(void* key);

  // Copies contents of |other| set into the current set. The current set
  // must be empty before the call.
  // Concurrent execution of |Insert|, |Remove|, or |Copy| is not supported.
  void Copy(const LockFreeAddressHashSet& other);

  size_t buckets_count() const {
    // `buckets_` should never be resized.
    DCHECK_EQ(buckets_.size(), bucket_mask_ + 1);
    return buckets_.size();
  }

  size_t size() const {
    lock_->AssertAcquired();
    return size_;
  }

  // Returns the average bucket utilization.
  float load_factor() const {
    lock_->AssertAcquired();
    return 1.f * size() / buckets_.size();
  }

  // Returns stats about the buckets. Must not be called concurrently with
  // |Insert|, |Remove| or |Copy|.
  BucketStats GetBucketStats() const;

 private:
  friend class LockFreeAddressHashSetTest;

  static void* const kDeletedKey;

  using KeySlot = std::atomic<void*>;

  class Node {
   public:
    // This field is not a raw_ptr<> to avoid out-of-line destructor.
    RAW_PTR_EXCLUSION Node* next;

   protected:
    // Can only be created through subclasses.
    ALWAYS_INLINE explicit Node(Node* next) : next(next) {}
  };

  class SingleKeyNode : public Node {
   public:
    ALWAYS_INLINE SingleKeyNode(void* k, Node* next) : Node(next) {
      key.store(k, std::memory_order_relaxed);
    }

    KeySlot key;
  };

  template <size_t N>
  class KeyArrayNode : public Node {
   public:
    static constexpr bool kFitsInCacheLine =
        sizeof(KeyArrayNode) <= std::hardware_constructive_interference_size;

    ALWAYS_INLINE KeyArrayNode(void* k, Node* next) : Node(next) {
      keys.front().store(k, std::memory_order_relaxed);
    }

    std::array<KeySlot, N> keys{};
  };

  // For the median client, the 50th %ile of bucket chain length ranges from 0.6
  // nodes to 2.6 nodes, depending on platform and process type. The 99th %ile
  // ranges from 1.6 nodes to 4.6 nodes. So 4-node chunks is a good choice to
  // maximize locality without wasting too much unused space. However the chosen
  // length should fit in a single cache line; if not fall back to smaller
  // chunks.
  static constexpr size_t kKeysPerNode =
      KeyArrayNode<4>::kFitsInCacheLine
          ? 4
          : (KeyArrayNode<2>::kFitsInCacheLine ? 2 : 1);
  using MultiKeyNode = KeyArrayNode<kKeysPerNode>;

  // Returns the KeySlot containing `key` (which must not be null), or nullptr
  // if it's not in the hash set.
  ALWAYS_INLINE KeySlot* FindKey(void* key);
  ALWAYS_INLINE const KeySlot* FindKey(void* key) const;

  // Returns a view of all key slots in `node`.
  ALWAYS_INLINE base::span<KeySlot> GetKeySlots(Node* node);
  ALWAYS_INLINE base::span<const KeySlot> GetKeySlots(const Node* node) const;

  // Returns the hash of `key`.
  ALWAYS_INLINE static uint32_t Hash(void* key);

  raw_ref<Lock> lock_;

  std::vector<std::atomic<Node*>> buckets_;
  size_t size_ GUARDED_BY(lock_) = 0;
  const size_t bucket_mask_;
  const bool multi_key_;
};

ALWAYS_INLINE bool LockFreeAddressHashSet::Contains(void* key) const {
  return FindKey(key) != nullptr;
}

ALWAYS_INLINE void LockFreeAddressHashSet::Remove(void* key) {
  lock_->AssertAcquired();
  KeySlot* key_slot = FindKey(key);
  DCHECK_NE(key_slot, nullptr);
  // Mark the key slot as empty, so |Insert| can reuse it later.
  key_slot->store(kDeletedKey, std::memory_order_relaxed);
  // The node may now be empty, but we can never delete it, nor detach it from
  // the current bucket as there may always be another thread currently
  // iterating over it.
  --size_;
}

ALWAYS_INLINE LockFreeAddressHashSet::KeySlot* LockFreeAddressHashSet::FindKey(
    void* key) {
  DCHECK_NE(key, nullptr);
  DCHECK_NE(key, kDeletedKey);
  const std::atomic<Node*>& bucket = buckets_[Hash(key) & bucket_mask_];
  // It's enough to use std::memory_order_consume ordering here, as the
  // node->next->...->next loads form a dependency chain.
  // However std::memory_order_consume is temporarily deprecated in C++17.
  // See https://isocpp.org/files/papers/p0636r0.html#removed
  // Make use of more strong std::memory_order_acquire for now.
  //
  // Update 2024-12-13: According to
  // https://en.cppreference.com/w/cpp/atomic/memory_order, C++20 changed the
  // semantics of a "consume operation" - see the definitions of
  // "Dependency-ordered before", "Simply happens-before" and "Strongly
  // happens-before" - but "Release-Consume ordering" still carries the note
  // that it's "temporarily discouraged" so it's unclear if it's now safe to use
  // here.
  for (Node* node = bucket.load(std::memory_order_acquire); node != nullptr;
       node = node->next) {
    for (KeySlot& key_slot : GetKeySlots(node)) {
      void* key_in_slot = key_slot.load(std::memory_order_relaxed);
      if (key_in_slot == key) {
        return &key_slot;
      } else if (key_in_slot == nullptr) {
        // Remaining slots in this node are empty.
        break;
      }
    }
  }
  return nullptr;
}

ALWAYS_INLINE const LockFreeAddressHashSet::KeySlot*
LockFreeAddressHashSet::FindKey(void* key) const {
  return const_cast<LockFreeAddressHashSet*>(this)->FindKey(key);
}

ALWAYS_INLINE base::span<LockFreeAddressHashSet::KeySlot>
LockFreeAddressHashSet::GetKeySlots(Node* node) {
  if (multi_key_) {
    return base::span(reinterpret_cast<MultiKeyNode*>(node)->keys);
  } else {
    return base::span_from_ref(reinterpret_cast<SingleKeyNode*>(node)->key);
  }
}

ALWAYS_INLINE base::span<const LockFreeAddressHashSet::KeySlot>
LockFreeAddressHashSet::GetKeySlots(const Node* node) const {
  if (multi_key_) {
    return base::span(reinterpret_cast<const MultiKeyNode*>(node)->keys);
  } else {
    return base::span_from_ref(
        reinterpret_cast<const SingleKeyNode*>(node)->key);
  }
}

// static
ALWAYS_INLINE uint32_t LockFreeAddressHashSet::Hash(void* key) {
  // A simple fast hash function for addresses.
  constexpr uintptr_t random_bits = static_cast<uintptr_t>(0x4bfdb9df5a6f243b);
  uint64_t k = reinterpret_cast<uintptr_t>(key);
  return static_cast<uint32_t>((k * random_bits) >> 32);
}

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_ADDRESS_HASH_SET_H_
