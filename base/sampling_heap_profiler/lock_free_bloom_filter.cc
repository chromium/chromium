// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_bloom_filter.h"

#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include "base/check_op.h"
#include "base/compiler_specific.h"
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
    const size_t hash =
        use_fake_hash_functions ? (int_ptr >> i) : base::HashInts(int_ptr, i);
    // Use explicit unsigned constant because `1 << n` may sign-extend if
    // size_t is narrower than BitStorage.
    bitmask |= 1u << (hash % LockFreeBloomFilter::kMaxBits);
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

LockFreeBloomFilter::BitStorage LockFreeBloomFilter::GetBitsForTesting() const {
  return bits_.load(std::memory_order_relaxed);
}

void LockFreeBloomFilter::SetBitsForTesting(const BitStorage& bits) {
  bits_.store(bits, std::memory_order_relaxed);
}

}  // namespace base
