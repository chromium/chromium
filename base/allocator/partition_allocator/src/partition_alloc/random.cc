// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/random.h"

#include <type_traits>

#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc {

class RandomGenerator {
 public:
  constexpr RandomGenerator() {}

  uint32_t RandomValue() {
    ::partition_alloc::internal::ScopedGuard guard(lock_);
    return GetGenerator()->RandUint32();
  }

  void SeedForTesting(uint64_t seed) {
    ::partition_alloc::internal::ScopedGuard guard(lock_);
    GetGenerator()->ReseedForTesting(seed);
  }

 private:
  ::partition_alloc::internal::Lock lock_ = {};
  bool initialized_ PA_GUARDED_BY(lock_) = false;
  union {
    internal::base::InsecureRandomGenerator instance_ PA_GUARDED_BY(lock_);
    uint8_t instance_buffer_[sizeof(
        internal::base::InsecureRandomGenerator)] PA_GUARDED_BY(lock_) = {};
  };

  internal::base::InsecureRandomGenerator* GetGenerator()
      PA_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    if (!initialized_) {
      new (instance_buffer_) internal::base::InsecureRandomGenerator();
      initialized_ = true;
    }
    return &instance_;
  }
};

// Note: this is redundant, since the anonymous union is incompatible with a
// non-trivial default destructor. Not meant to be destructed anyway.
static_assert(std::is_trivially_destructible_v<RandomGenerator>, "");

namespace {

RandomGenerator g_generator = {};

}  // namespace

namespace internal {

uint32_t RandomValue() {
  return g_generator.RandomValue();
}

}  // namespace internal

void SetMmapSeedForTesting(uint64_t seed) {
  return g_generator.SeedForTesting(seed);
}

}  // namespace partition_alloc
