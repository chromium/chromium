// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/random.h"

#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/rand_util.h"

namespace base {

namespace partition_alloc {
class RandomGenerator {
 public:
  RandomGenerator() = default;
  uint32_t RandomValue() {
    internal::ScopedGuard<true> guard(lock_);
    if (!generator_.seeded())
      generator_.Seed();

    return generator_.RandUint32();
  }

  void SeedForTesting(uint64_t seed) {
    internal::ScopedGuard<true> guard(lock_);
    generator_.SeedForTesting(seed);
  }

 private:
  internal::PartitionLock lock_ = {};
  InsecureRandomGenerator generator_ GUARDED_BY(lock_) = {};
};
}  // namespace partition_alloc

namespace {

partition_alloc::RandomGenerator g_generator = {};

}  // namespace

uint32_t RandomValue() {
  return g_generator.RandomValue();
}

void SetMmapSeedForTesting(uint64_t seed) {
  return g_generator.SeedForTesting(seed);
}

}  // namespace base
