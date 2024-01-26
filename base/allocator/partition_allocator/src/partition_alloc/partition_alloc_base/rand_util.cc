// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/rand_util.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <limits>

#include "partition_alloc/partition_alloc_base/check.h"

namespace partition_alloc::internal::base {

uint64_t RandUint64() {
  uint64_t number;
  RandBytes(&number, sizeof(number));
  return number;
}

uint64_t RandGenerator(uint64_t range) {
  PA_BASE_DCHECK(range > 0u);
  // We must discard random results above this number, as they would
  // make the random generator non-uniform (consider e.g. if
  // MAX_UINT64 was 7 and |range| was 5, then a result of 1 would be twice
  // as likely as a result of 3 or 4).
  uint64_t max_acceptable_value =
      (std::numeric_limits<uint64_t>::max() / range) * range - 1;

  uint64_t value;
  do {
    value = base::RandUint64();
  } while (value > max_acceptable_value);

  return value % range;
}

InsecureRandomGenerator::InsecureRandomGenerator() {
  a_ = base::RandUint64();
  b_ = base::RandUint64();
}

void InsecureRandomGenerator::ReseedForTesting(uint64_t seed) {
  a_ = seed;
  b_ = seed;
}

uint64_t InsecureRandomGenerator::RandUint64() {
  // Using XorShift128+, which is simple and widely used. See
  // https://en.wikipedia.org/wiki/Xorshift#xorshift+ for details.
  uint64_t t = a_;
  const uint64_t s = b_;

  a_ = s;
  t ^= t << 23;
  t ^= t >> 17;
  t ^= s ^ (s >> 26);
  b_ = t;

  return t + s;
}

uint32_t InsecureRandomGenerator::RandUint32() {
  // The generator usually returns an uint64_t, truncate it.
  //
  // It is noted in this paper (https://arxiv.org/abs/1810.05313) that the
  // lowest 32 bits fail some statistical tests from the Big Crush
  // suite. Use the higher ones instead.
  return this->RandUint64() >> 32;
}

}  // namespace partition_alloc::internal::base
