// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_BLOOM_FILTER_H_
#define BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_BLOOM_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <bit>
#include <bitset>

#include "base/containers/span.h"
#include "base/hash/hash.h"

namespace base {

// A fixed-size Bloom filter for keeping track of a set of pointers, that can be
// read and written from multiple threads at the same time. The number of bits
// is fixed so that all bits can be held in an atomic integer.
//
// The implementation was inspired by
// components/optimization_guide/core/bloom_filter.h, modified to be thread-safe
// and take void* arguments.
//
// A Bloom filter can determine precisely that a pointer is NOT in the set, but
// hash collisions make it impossible to be certain that a given pointer IS in
// the set. (That is, MaybeContains(ptr) can return false positives but never
// false negatives.) It's intended to be used in front of LockFreeAddressHashSet
// to optimize the common case of looking up a pointer that's not in the hash
// set, as follows:
//
//  To add a key:
//    bloom_filter.Add(ptr);
//    hash_set.Insert(ptr);
//
//  To look up a key:
//    if (bloom_filter.MaybeContains(ptr)) {
//      if (hash_set.Contains(ptr)) {
//        ... Do something.
//      } else {
//        ... False positive. Do nothing.
//      }
//    } else {
//      ... Not in bloom_filter, so not in hash_set. Do nothing.
//    }
//
// This class only guarantees that accessing the Bloom filter is thread-safe.
// The caller is responsible for ensuring that accessing both the filter and
// LockFreeAddressHashSet from multiple threads gives consistent results.
//
// See lock_free_address_hash_set.cc for a table of estimated false positive
// rates when the filter is used with LockFreeAddressHashSet.

// An integer to hold the bits that are set in the filter.
using LockFreeBloomFilterBits = uint64_t;

// Maximum number of bits in the filter.
static constexpr size_t kMaxLockFreeBloomFilterBits =
    8 * sizeof(LockFreeBloomFilterBits);

// A bloom filter of `kMaxLockFreeBloomFilterBits` size that sets up to
// `BitsPerKey` bits per entry. Each key added to the filter is hashed with
// `BitsPerKey` separate hash functions, each of which returns the index of a
// bit to set. (Fewer bits can be set for a given entry if multiple hash
// functions return the same index.)
//
// If UseFakeHashFunctionsForTesting is `true`, hashing a key with hash function
// N will shift the key N bits to the right, allowing the test to precisely
// control how keys are hashed.
template <size_t BitsPerKey, bool UseFakeHashFunctionsForTesting = false>
class LockFreeBloomFilter {
 public:
  LockFreeBloomFilter() = default;
  ~LockFreeBloomFilter() = default;

  LockFreeBloomFilter(const LockFreeBloomFilter&) = delete;
  LockFreeBloomFilter& operator=(const LockFreeBloomFilter&) = delete;

  // Returns the bits corresponding to `ptr` in this Bloom filter.
  LockFreeBloomFilterBits GetBitsForKey(void* ptr) const;

  // Returns whether `ptr` may have been added as a key in this Bloom filter. If
  // this returns false, `ptr` is definitely not in the filter. Otherwise `ptr`
  // may or may not be in the filter, since Bloom filters inherently have false
  // positives. (See the table of estimated false positive rates above.)
  bool MaybeContains(void* ptr) const {
    // `ptr` is potentially in the filter iff ALL bits in the bitmask are set.
    const LockFreeBloomFilterBits bitmask = GetBitsForKey(ptr);
    return (bits_.load(std::memory_order_relaxed) & bitmask) == bitmask;
  }

  // Adds `ptr` as a key in this Bloom filter. After this call
  // MaybeContains(ptr) will always return true.
  void Add(void* ptr) {
    bits_.fetch_or(GetBitsForKey(ptr), std::memory_order_relaxed);
  }

  // Atomically overwrites the bloom filter with `bits`.
  void AtomicSetBits(LockFreeBloomFilterBits bits) {
    bits_.store(bits, std::memory_order_relaxed);
  }

  // Returns the bit array data of this Bloom filter as an integer.
  LockFreeBloomFilterBits GetBitsForTesting() const {
    return bits_.load(std::memory_order_relaxed);
  }

  // Returns the number of bits that are set, for stats.
  size_t CountBits() const {
    return std::bitset<kMaxLockFreeBloomFilterBits>(
               bits_.load(std::memory_order_relaxed))
        .count();
  }

 private:
  // Bit data for the filter.
  // Accessed with std::memory_order_relaxed since the class doesn't synchronize
  // access to pointed-to memory. Instead pointers passed to Add() are treated
  // as opaque keys.
  static_assert(std::atomic<LockFreeBloomFilterBits>::is_always_lock_free);
  std::atomic<LockFreeBloomFilterBits> bits_ = 0;
};

template <size_t BitsPerKey, bool UseFakeHashFunctionsForTesting>
LockFreeBloomFilterBits
LockFreeBloomFilter<BitsPerKey, UseFakeHashFunctionsForTesting>::GetBitsForKey(
    void* ptr) const {
  // If LockFreeBloomFilterBits is N bits wide, then an evenly-distributed hash
  // function can be used as an index into it by masking off the last N-1 bits.
  // (Equivalent to mod N when N is divisible by 2.)
  static_assert(std::has_single_bit(kMaxLockFreeBloomFilterBits),
                "kMaxLockFreeBloomFilterBits must be divisible by 2");

  constexpr size_t kIndexMask = kMaxLockFreeBloomFilterBits - 1;
  constexpr size_t kShiftWidth = std::bit_width(kIndexMask);
  static_assert(BitsPerKey * kShiftWidth <= 8 * sizeof(size_t),
                "Must be able to mask off `BitsPerKey` subsets of a size_t");

  LockFreeBloomFilterBits bitmask = 0;
  const uintptr_t int_ptr = reinterpret_cast<uintptr_t>(ptr);
  size_t hash = base::FastHash(base::as_byte_span({int_ptr}));
  for (size_t i = 0; i < BitsPerKey; ++i) {
    if constexpr (UseFakeHashFunctionsForTesting) {
      // Overwrite the previous hash with a fake.
      hash = int_ptr >> i;
    }
    // Use the last bits of the hash as an index, and set the bit at that index.
    // Make sure the argument of << is the same type as `bitmask` to avoid
    // undefined behaviour on overflow if size_t is narrower.
    constexpr LockFreeBloomFilterBits kOneBit = 1;
    bitmask |= kOneBit << (hash & kIndexMask);

    // Shift the used bits out of the hash.
    hash >>= kShiftWidth;
  }
  return bitmask;
}

}  // namespace base

#endif  // BASE_SAMPLING_HEAP_PROFILER_LOCK_FREE_BLOOM_FILTER_H_
