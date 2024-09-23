// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <limits>

#include "base/check_op.h"
#include "base/time/time.h"

namespace base {

namespace {

// A MetricSubsampler instance is not thread-safe. However, the global
// sampling state may be read concurrently with writing it via testing
// scopers, hence the need to use atomics. All operations use
// memory_order_relaxed because there are no dependent memory accesses.
std::atomic<bool> g_subsampling_always_sample = false;
std::atomic<bool> g_subsampling_never_sample = false;

}  // namespace

uint64_t RandUint64() {
  uint64_t number;
  RandBytes(base::byte_span_from_ref(number));
  return number;
}

int RandInt(int min, int max) {
  DCHECK_LE(min, max);

  uint64_t range = static_cast<uint64_t>(max) - static_cast<uint64_t>(min) + 1;
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

float RandFloat() {
  return BitsToOpenEndedUnitIntervalF(base::RandUint64());
}

TimeDelta RandTimeDelta(TimeDelta start, TimeDelta limit) {
  // We must have a finite, non-empty, non-reversed interval.
  CHECK_LT(start, limit);
  CHECK(!start.is_min());
  CHECK(!limit.is_max());

  const int64_t range = (limit - start).InMicroseconds();
  // Because of the `CHECK_LT()` above, range > 0, so this cast is safe.
  const uint64_t delta_us = base::RandGenerator(static_cast<uint64_t>(range));
  // ...and because `range` fit in an `int64_t`, so will `delta_us`.
  return start + Microseconds(static_cast<int64_t>(delta_us));
}

TimeDelta RandTimeDeltaUpTo(TimeDelta limit) {
  return RandTimeDelta(TimeDelta(), limit);
}

double BitsToOpenEndedUnitInterval(uint64_t bits) {
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 doubles, the mantissa
  // is expected to accommodate 53 bits (including the implied bit).
  static_assert(std::numeric_limits<double>::radix == 2,
                "otherwise use scalbn");
  constexpr int kBits = std::numeric_limits<double>::digits;
  return ldexp(bits & ((UINT64_C(1) << kBits) - 1u), -kBits);
}

float BitsToOpenEndedUnitIntervalF(uint64_t bits) {
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 floats, the mantissa is
  // expected to accommodate 12 bits (including the implied bit).
  static_assert(std::numeric_limits<float>::radix == 2, "otherwise use scalbn");
  constexpr int kBits = std::numeric_limits<float>::digits;
  return ldexpf(bits & ((UINT64_C(1) << kBits) - 1u), -kBits);
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
  std::string result(length, '\0');
  RandBytes(as_writable_byte_span(result));
  return result;
}

std::vector<uint8_t> RandBytesAsVector(size_t length) {
  std::vector<uint8_t> result(length);
  RandBytes(result);
  return result;
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

double InsecureRandomGenerator::RandDouble() {
  uint64_t x = RandUint64();
  // From https://vigna.di.unimi.it/xorshift/.
  // 53 bits of mantissa, hence the "hexadecimal exponent" 1p-53.
  return (x >> 11) * 0x1.0p-53;
}

MetricsSubSampler::MetricsSubSampler() = default;
bool MetricsSubSampler::ShouldSample(double probability) {
  if (g_subsampling_always_sample.load(std::memory_order_relaxed)) {
    return true;
  }
  if (g_subsampling_never_sample.load(std::memory_order_relaxed)) {
    return false;
  }

  return generator_.RandDouble() < probability;
}

MetricsSubSampler::ScopedAlwaysSampleForTesting::
    ScopedAlwaysSampleForTesting() {
  DCHECK(!g_subsampling_always_sample.load(std::memory_order_relaxed));
  DCHECK(!g_subsampling_never_sample.load(std::memory_order_relaxed));
  g_subsampling_always_sample.store(true, std::memory_order_relaxed);
}

MetricsSubSampler::ScopedAlwaysSampleForTesting::
    ~ScopedAlwaysSampleForTesting() {
  DCHECK(g_subsampling_always_sample.load(std::memory_order_relaxed));
  DCHECK(!g_subsampling_never_sample.load(std::memory_order_relaxed));
  g_subsampling_always_sample.store(false, std::memory_order_relaxed);
}

MetricsSubSampler::ScopedNeverSampleForTesting::ScopedNeverSampleForTesting() {
  DCHECK(!g_subsampling_always_sample.load(std::memory_order_relaxed));
  DCHECK(!g_subsampling_never_sample.load(std::memory_order_relaxed));
  g_subsampling_never_sample.store(true, std::memory_order_relaxed);
}

MetricsSubSampler::ScopedNeverSampleForTesting::~ScopedNeverSampleForTesting() {
  DCHECK(!g_subsampling_always_sample);
  DCHECK(g_subsampling_never_sample);
  g_subsampling_never_sample.store(false, std::memory_order_relaxed);
}

}  // namespace base
