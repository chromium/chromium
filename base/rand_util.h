// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RAND_UTIL_H_
#define BASE_RAND_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"

namespace blink {
namespace scheduler {
class UkmTaskSampler;
class MainThreadMetricsHelper;
}
}  // namespace blink

namespace base {

// Returns a random number in range [0, UINT64_MAX]. Thread-safe.
BASE_EXPORT uint64_t RandUint64();

// Returns a random number between min and max (inclusive). Thread-safe.
BASE_EXPORT int RandInt(int min, int max);

// Returns a random number in range [0, range).  Thread-safe.
BASE_EXPORT uint64_t RandGenerator(uint64_t range);

// Returns a random double in range [0, 1). Thread-safe.
BASE_EXPORT double RandDouble();

// Given input |bits|, convert with maximum precision to a double in
// the range [0, 1). Thread-safe.
BASE_EXPORT double BitsToOpenEndedUnitInterval(uint64_t bits);

// Fills |output_length| bytes of |output| with random data. Thread-safe.
//
// Although implementations are required to use a cryptographically secure
// random number source, code outside of base/ that relies on this should use
// crypto::RandBytes instead to ensure the requirement is easily discoverable.
BASE_EXPORT void RandBytes(void* output, size_t output_length);

// Fills a string of length |length| with random data and returns it.
// |length| should be nonzero. Thread-safe.
//
// Note that this is a variation of |RandBytes| with a different return type.
// The returned string is likely not ASCII/UTF-8. Use with care.
//
// Although implementations are required to use a cryptographically secure
// random number source, code outside of base/ that relies on this should use
// crypto::RandBytes instead to ensure the requirement is easily discoverable.
BASE_EXPORT std::string RandBytesAsString(size_t length);

// An STL UniformRandomBitGenerator backed by RandUint64.
// TODO(tzik): Consider replacing this with a faster implementation.
class RandomBitGenerator {
 public:
  using result_type = uint64_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return UINT64_MAX; }
  result_type operator()() const { return RandUint64(); }

  RandomBitGenerator() = default;
  ~RandomBitGenerator() = default;
};

// Shuffles [first, last) randomly. Thread-safe.
template <typename Itr>
void RandomShuffle(Itr first, Itr last) {
  std::shuffle(first, last, RandomBitGenerator());
}

#if defined(OS_POSIX)
BASE_EXPORT int GetUrandomFD();
#endif

namespace partition_alloc {
class RandomGenerator;
}

namespace sequence_manager {
namespace internal {
class SequenceManagerImpl;
}
}  // namespace sequence_manager

// Fast, insecure pseudo-random number generator.
//
// WARNING: This is not the generator you are looking for. This has significant
// caveats:
//   - It is non-cryptographic, so easy to miuse
//   - It is neither fork() nor clone()-safe.
//   - Synchronization is up to the client.
//
// Always prefer base::Rand*() above, unless you have a use case where its
// overhead is too high, or system calls are disallowed.
//
// Performance: As of 2021, rough overhead on Linux on a desktop machine of
// base::RandUint64() is ~800ns per call (it performs a system call). On Windows
// it is lower. On the same machine, this generator's cost is ~2ns per call,
// regardless of platform.
//
// This is different from |Rand*()| above as it is guaranteed to never make a
// system call to generate a new number, except to seed it.  This should *never*
// be used for cryptographic applications, and is not thread-safe.
//
// It must be seeded before use with |Seed()|, but the period is long enough to
// not require re-seeding. Nevertheless, seeding the generator multiple times is
// harmless.
//
// Uses the XorShift128+ generator under the hood.
class BASE_EXPORT InsecureRandomGenerator {
 public:
  // Sets the seed by calling RandUint64() to initialize internal state.
  void Seed();
  bool seeded() const { return seeded_; }

  // Never use outside testing, not enough entropy.
  void SeedForTesting(uint64_t seed);

  uint32_t RandUint32();
  uint64_t RandUint64();
  // In [0, 1).
  double RandDouble();

 private:
  InsecureRandomGenerator() = default;

  bool seeded_ = false;
  // State.
  uint64_t a_ = 0, b_ = 0;

  // Before adding a new friend class, make sure that the overhead of
  // base::Rand*() is too high, using something more representative than a
  // microbenchmark.
  //
  // PartitionAlloc allocations should not take more than 40-50ns per
  // malloc()/free() pair, otherwise high-level benchmarks regress, and does not
  // need a secure PRNG, as it's used for ASLR and zeroing some allocations at
  // free() time.
  friend class partition_alloc::RandomGenerator;

  // Friend classes below are using the generator to sub-sample metrics after
  // task execution. Task execution overhead is ~1us on a Linux desktop, and yet
  // accounts for multiple percentage points of total CPU usage. Keeping it low
  // is thus important.
  friend class sequence_manager::internal::SequenceManagerImpl;
  friend class blink::scheduler::UkmTaskSampler;
  friend class blink::scheduler::MainThreadMetricsHelper;

  FRIEND_TEST_ALL_PREFIXES(RandUtilTest,
                           InsecureRandomGeneratorProducesBothValuesOfAllBits);
  FRIEND_TEST_ALL_PREFIXES(RandUtilTest, InsecureRandomGeneratorChiSquared);
  FRIEND_TEST_ALL_PREFIXES(RandUtilTest, InsecureRandomGeneratorRandDouble);
  FRIEND_TEST_ALL_PREFIXES(RandUtilPerfTest, InsecureRandomRandUint64);
};

}  // namespace base

#endif  // BASE_RAND_UTIL_H_
