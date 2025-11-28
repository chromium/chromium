// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_bloom_filter.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <atomic>
#include <bitset>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"

namespace base {

namespace {

ALWAYS_INLINE LockFreeBloomFilter::BitStorage CreateBitmask(
    void* ptr,
    size_t num_hash_functions,
    bool use_fake_hash_functions) {
  LockFreeBloomFilter::BitStorage bitmask = 0;
  const uintptr_t int_ptr = reinterpret_cast<uintptr_t>(ptr);
  for (size_t i = 0; i < num_hash_functions; ++i) {
    std::array<uintptr_t, 2> hash_input{int_ptr, i};
    const size_t hash = use_fake_hash_functions
                            ? (int_ptr >> i)
                            : base::FastHash(base::as_byte_span(hash_input));
    // Make sure the argument of << is the same type as `bitmask` to avoid
    // undefined behaviour on overflow if size_t is narrower.
    static constexpr LockFreeBloomFilter::BitStorage kOneBit = 1;
    bitmask |= kOneBit << (hash % LockFreeBloomFilter::kMaxBits);
  }
  return bitmask;
}

}  // namespace

LockFreeBloomFilter::LockFreeBloomFilter(size_t num_hash_functions)
    : num_hash_functions_(num_hash_functions) {}

LockFreeBloomFilter::~LockFreeBloomFilter() = default;

bool LockFreeBloomFilter::MaybeContains(void* ptr) const {
  // `ptr` is potentially in the filter iff ALL bits in the bitmask are set.
  const BitStorage bitmask =
      CreateBitmask(ptr, num_hash_functions_, use_fake_hash_functions_);
  return (bits_.load(std::memory_order_relaxed) & bitmask) == bitmask;
}

void LockFreeBloomFilter::Add(void* ptr) {
  const BitStorage bitmask =
      CreateBitmask(ptr, num_hash_functions_, use_fake_hash_functions_);
  bits_.fetch_or(bitmask, std::memory_order_relaxed);
}

void LockFreeBloomFilter::AtomicSetBits(BitStorage bits) {
  // memory_order_relaxed is sufficient because this function only guarantees
  // that `bits_` is updated atomically. If the caller has other data depending
  // on `bits_`, it's up to them to enforce ordering.
  bits_.store(bits, std::memory_order_relaxed);
}

LockFreeBloomFilter::BitStorage LockFreeBloomFilter::GetBitsForKey(
    void* ptr) const {
  return CreateBitmask(ptr, num_hash_functions_, use_fake_hash_functions_);
}

LockFreeBloomFilter::BitStorage LockFreeBloomFilter::GetBitsForTesting() const {
  return bits_.load(std::memory_order_relaxed);
}

size_t LockFreeBloomFilter::CountBits() const {
  // Relaxed ordering is enough since this is only for statistics.
  return std::bitset<kMaxBits>(bits_.load(std::memory_order_relaxed)).count();
}

}  // namespace base
