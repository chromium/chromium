// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/strings/string_util.h"

namespace base {

uint64_t RandUint64() {
  uint64_t number;
  RandBytes(&number, sizeof(number));
  return number;
}

int RandInt(int min, int max) {
  DCHECK_LE(min, max);

  uint64_t range = static_cast<uint64_t>(max) - min + 1;
  // |range| is at most UINT_MAX + 1, so the result of RandGenerator(range)
  // is at most UINT_MAX.  Hence it's safe to cast it from uint64_t to int64_t.
  int result =
      static_cast<int>(min + static_cast<int64_t>(base::RandGenerator(range)));
  DCHECK_GE(result, min);
  DCHECK_LE(result, max);
  return result;
}

double RandDouble() {
  return BitsToOpenEndedUnitInterval(base::RandUint64());
}

double BitsToOpenEndedUnitInterval(uint64_t bits) {
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 doubles, the mantissa
  // is expected to accommodate 53 bits.

  static_assert(std::numeric_limits<double>::radix == 2,
                "otherwise use scalbn");
  static const int kBits = std::numeric_limits<double>::digits;
  uint64_t random_bits = bits & ((UINT64_C(1) << kBits) - 1);
  double result = ldexp(static_cast<double>(random_bits), -1 * kBits);
  DCHECK_GE(result, 0.0);
  DCHECK_LT(result, 1.0);
  return result;
}

uint64_t RandGenerator(uint64_t range) {
  DCHECK_GT(range, 0u);
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

std::string RandBytesAsString(size_t length) {
  DCHECK_GT(length, 0u);
  std::string result;
  RandBytes(WriteInto(&result, length + 1), length);
  return result;
}

void InsecureRandomGenerator::Seed() {
  a_ = base::RandUint64();
  b_ = base::RandUint64();
  seeded_ = true;
}

void InsecureRandomGenerator::SeedForTesting(uint64_t seed) {
  a_ = seed;
  b_ = seed;
  seeded_ = true;
}

uint64_t InsecureRandomGenerator::RandUint64() {
  DCHECK(seeded_);

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

double InsecureRandomGenerator::RandDouble() {
  uint64_t x = RandUint64();
  // From https://vigna.di.unimi.it/xorshift/.
  // 53 bits of mantissa, hence the "hexadecimal exponent" 1p-53.
  return (x >> 11) * 0x1.0p-53;
}

}  // namespace base
