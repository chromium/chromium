// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/rand_util.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

const int kIntMin = std::numeric_limits<int>::min();
const int kIntMax = std::numeric_limits<int>::max();

}  // namespace

TEST(RandUtilTest, RandInt) {
  EXPECT_EQ(base::RandInt(0, 0), 0);
  EXPECT_EQ(base::RandInt(kIntMin, kIntMin), kIntMin);
  EXPECT_EQ(base::RandInt(kIntMax, kIntMax), kIntMax);

  // Check that the DCHECKS in RandInt() don't fire due to internal overflow.
  // There was a 50% chance of that happening, so calling it 40 times means
  // the chances of this passing by accident are tiny (9e-13).
  for (int i = 0; i < 40; ++i)
    base::RandInt(kIntMin, kIntMax);
}

TEST(RandUtilTest, RandDouble) {
  // Force 64-bit precision, making sure we're not in a 80-bit FPU register.
  volatile double number = base::RandDouble();
  EXPECT_GT(1.0, number);
  EXPECT_LE(0.0, number);
}

TEST(RandUtilTest, RandFloat) {
  // Force 32-bit precision, making sure we're not in an 80-bit FPU register.
  volatile float number = base::RandFloat();
  EXPECT_GT(1.f, number);
  EXPECT_LE(0.f, number);
}

TEST(RandUtilTest, RandTimeDelta) {
  {
    const auto delta =
        base::RandTimeDelta(-base::Seconds(2), -base::Seconds(1));
    EXPECT_GE(delta, -base::Seconds(2));
    EXPECT_LT(delta, -base::Seconds(1));
  }

  {
    const auto delta = base::RandTimeDelta(-base::Seconds(2), base::Seconds(2));
    EXPECT_GE(delta, -base::Seconds(2));
    EXPECT_LT(delta, base::Seconds(2));
  }

  {
    const auto delta = base::RandTimeDelta(base::Seconds(1), base::Seconds(2));
    EXPECT_GE(delta, base::Seconds(1));
    EXPECT_LT(delta, base::Seconds(2));
  }
}

TEST(RandUtilTest, RandTimeDeltaUpTo) {
  const auto delta = base::RandTimeDeltaUpTo(base::Seconds(2));
  EXPECT_FALSE(delta.is_negative());
  EXPECT_LT(delta, base::Seconds(2));
}

TEST(RandUtilTest, BitsToOpenEndedUnitInterval) {
  // Force 64-bit precision, making sure we're not in an 80-bit FPU register.
  volatile double all_zeros = BitsToOpenEndedUnitInterval(0x0);
  EXPECT_EQ(0.0, all_zeros);

  // Force 64-bit precision, making sure we're not in an 80-bit FPU register.
  volatile double smallest_nonzero = BitsToOpenEndedUnitInterval(0x1);
  EXPECT_LT(0.0, smallest_nonzero);

  for (uint64_t i = 0x2; i < 0x10; ++i) {
    // Force 64-bit precision, making sure we're not in an 80-bit FPU register.
    volatile double number = BitsToOpenEndedUnitInterval(i);
    EXPECT_EQ(i * smallest_nonzero, number);
  }

  // Force 64-bit precision, making sure we're not in an 80-bit FPU register.
  volatile double all_ones = BitsToOpenEndedUnitInterval(UINT64_MAX);
  EXPECT_GT(1.0, all_ones);
}

TEST(RandUtilTest, BitsToOpenEndedUnitIntervalF) {
  // Force 32-bit precision, making sure we're not in an 80-bit FPU register.
  volatile float all_zeros = BitsToOpenEndedUnitIntervalF(0x0);
  EXPECT_EQ(0.f, all_zeros);

  // Force 32-bit precision, making sure we're not in an 80-bit FPU register.
  volatile float smallest_nonzero = BitsToOpenEndedUnitIntervalF(0x1);
  EXPECT_LT(0.f, smallest_nonzero);

  for (uint64_t i = 0x2; i < 0x10; ++i) {
    // Force 32-bit precision, making sure we're not in an 80-bit FPU register.
    volatile float number = BitsToOpenEndedUnitIntervalF(i);
    EXPECT_EQ(i * smallest_nonzero, number);
  }

  // Force 32-bit precision, making sure we're not in an 80-bit FPU register.
  volatile float all_ones = BitsToOpenEndedUnitIntervalF(UINT64_MAX);
  EXPECT_GT(1.f, all_ones);
}

TEST(RandUtilTest, RandBytes) {
  const size_t buffer_size = 50;
  uint8_t buffer[buffer_size];
  memset(buffer, 0, buffer_size);
  base::RandBytes(buffer);
  std::sort(buffer, buffer + buffer_size);
  // Probability of occurrence of less than 25 unique bytes in 50 random bytes
  // is below 10^-25.
  EXPECT_GT(std::unique(buffer, buffer + buffer_size) - buffer, 25);
}

// Verify that calling base::RandBytes with an empty buffer doesn't fail.
TEST(RandUtilTest, RandBytes0) {
  base::RandBytes(span<uint8_t>());
}

TEST(RandUtilTest, RandBytesAsVector) {
  std::vector<uint8_t> random_vec = base::RandBytesAsVector(0);
  EXPECT_TRUE(random_vec.empty());
  random_vec = base::RandBytesAsVector(1);
  EXPECT_EQ(1U, random_vec.size());
  random_vec = base::RandBytesAsVector(145);
  EXPECT_EQ(145U, random_vec.size());
  char accumulator = 0;
  for (auto i : random_vec) {
    accumulator |= i;
  }
  // In theory this test can fail, but it won't before the universe dies of
  // heat death.
  EXPECT_NE(0, accumulator);
}

TEST(RandUtilTest, RandBytesAsString) {
  std::string random_string = base::RandBytesAsString(1);
  EXPECT_EQ(1U, random_string.size());
  random_string = base::RandBytesAsString(145);
  EXPECT_EQ(145U, random_string.size());
  char accumulator = 0;
  for (auto i : random_string)
    accumulator |= i;
  // In theory this test can fail, but it won't before the universe dies of
  // heat death.
  EXPECT_NE(0, accumulator);
}

// Make sure that it is still appropriate to use RandGenerator in conjunction
// with std::random_shuffle().
TEST(RandUtilTest, RandGeneratorForRandomShuffle) {
  EXPECT_EQ(base::RandGenerator(1), 0U);
  EXPECT_LE(std::numeric_limits<ptrdiff_t>::max(),
            std::numeric_limits<int64_t>::max());
}

TEST(RandUtilTest, RandGeneratorIsUniform) {
  // Verify that RandGenerator has a uniform distribution. This is a
  // regression test that consistently failed when RandGenerator was
  // implemented this way:
  //
  //   return base::RandUint64() % max;
  //
  // A degenerate case for such an implementation is e.g. a top of
  // range that is 2/3rds of the way to MAX_UINT64, in which case the
  // bottom half of the range would be twice as likely to occur as the
  // top half. A bit of calculus care of jar@ shows that the largest
  // measurable delta is when the top of the range is 3/4ths of the
  // way, so that's what we use in the test.
  constexpr uint64_t kTopOfRange =
      (std::numeric_limits<uint64_t>::max() / 4ULL) * 3ULL;
  constexpr double kExpectedAverage = static_cast<double>(kTopOfRange / 2);
  constexpr double kAllowedVariance = kExpectedAverage / 50.0;  // +/- 2%
  constexpr int kMinAttempts = 1000;
  constexpr int kMaxAttempts = 1000000;

  double cumulative_average = 0.0;
  int count = 0;
  while (count < kMaxAttempts) {
    uint64_t value = base::RandGenerator(kTopOfRange);
    cumulative_average = (count * cumulative_average + value) / (count + 1);

    // Don't quit too quickly for things to start converging, or we may have
    // a false positive.
    if (count > kMinAttempts &&
        kExpectedAverage - kAllowedVariance < cumulative_average &&
        cumulative_average < kExpectedAverage + kAllowedVariance) {
      break;
    }

    ++count;
  }

  ASSERT_LT(count, kMaxAttempts) << "Expected average was " << kExpectedAverage
                                 << ", average ended at " << cumulative_average;
}

TEST(RandUtilTest, RandUint64ProducesBothValuesOfAllBits) {
  // This tests to see that our underlying random generator is good
  // enough, for some value of good enough.
  uint64_t kAllZeros = 0ULL;
  uint64_t kAllOnes = ~kAllZeros;
  uint64_t found_ones = kAllZeros;
  uint64_t found_zeros = kAllOnes;

  for (size_t i = 0; i < 1000; ++i) {
    uint64_t value = base::RandUint64();
    found_ones |= value;
    found_zeros &= value;

    if (found_zeros == kAllZeros && found_ones == kAllOnes)
      return;
  }

  FAIL() << "Didn't achieve all bit values in maximum number of tries.";
}

TEST(RandUtilTest, RandBytesLonger) {
  // Fuchsia can only retrieve 256 bytes of entropy at a time, so make sure we
  // handle longer requests than that.
  std::string random_string0 = base::RandBytesAsString(255);
  EXPECT_EQ(255u, random_string0.size());
  std::string random_string1 = base::RandBytesAsString(1023);
  EXPECT_EQ(1023u, random_string1.size());
  std::string random_string2 = base::RandBytesAsString(4097);
  EXPECT_EQ(4097u, random_string2.size());
}

// Benchmark test for RandBytes().  Disabled since it's intentionally slow and
// does not test anything that isn't already tested by the existing RandBytes()
// tests.
TEST(RandUtilTest, DISABLED_RandBytesPerf) {
  // Benchmark the performance of |kTestIterations| of RandBytes() using a
  // buffer size of |kTestBufferSize|.
  const int kTestIterations = 10;
  const size_t kTestBufferSize = 1 * 1024 * 1024;

  std::array<uint8_t, kTestBufferSize> buffer;
  const base::TimeTicks now = base::TimeTicks::Now();
  for (int i = 0; i < kTestIterations; ++i) {
    base::RandBytes(buffer);
  }
  const base::TimeTicks end = base::TimeTicks::Now();

  LOG(INFO) << "RandBytes(" << kTestBufferSize
            << ") took: " << (end - now).InMicroseconds() << "µs";
}

TEST(RandUtilTest, InsecureRandomGeneratorProducesBothValuesOfAllBits) {
  // This tests to see that our underlying random generator is good
  // enough, for some value of good enough.
  uint64_t kAllZeros = 0ULL;
  uint64_t kAllOnes = ~kAllZeros;
  uint64_t found_ones = kAllZeros;
  uint64_t found_zeros = kAllOnes;

  InsecureRandomGenerator generator;

  for (size_t i = 0; i < 1000; ++i) {
    uint64_t value = generator.RandUint64();
    found_ones |= value;
    found_zeros &= value;

    if (found_zeros == kAllZeros && found_ones == kAllOnes)
      return;
  }

  FAIL() << "Didn't achieve all bit values in maximum number of tries.";
}

namespace {

constexpr double kXp1Percent = -2.33;
constexpr double kXp99Percent = 2.33;

double ChiSquaredCriticalValue(double nu, double x_p) {
  // From "The Art Of Computer Programming" (TAOCP), Volume 2, Section 3.3.1,
  // Table 1. This is the asymptotic value for nu > 30, up to O(1 / sqrt(nu)).
  return nu + sqrt(2. * nu) * x_p + 2. / 3. * (x_p * x_p) - 2. / 3.;
}

int ExtractBits(uint64_t value, int from_bit, int num_bits) {
  return (value >> from_bit) & ((1 << num_bits) - 1);
}

// Performs a Chi-Squared test on a subset of |num_bits| extracted starting from
// |from_bit| in the generated value.
//
// See TAOCP, Volume 2, Section 3.3.1, and
// https://en.wikipedia.org/wiki/Pearson%27s_chi-squared_test for details.
//
// This is only one of the many, many random number generator test we could do,
// but they are cumbersome, as they are typically very slow, and expected to
// fail from time to time, due to their probabilistic nature.
//
// The generator we use has however been vetted with the BigCrush test suite
// from Marsaglia, so this should suffice as a smoke test that our
// implementation is wrong.
bool ChiSquaredTest(InsecureRandomGenerator& gen,
                    size_t n,
                    int from_bit,
                    int num_bits) {
  const int range = 1 << num_bits;
  CHECK_EQ(static_cast<int>(n % range), 0) << "Makes computations simpler";
  std::vector<size_t> samples(range, 0);

  // Count how many samples pf each value are found. All buckets should be
  // almost equal if the generator is suitably uniformly random.
  for (size_t i = 0; i < n; i++) {
    int sample = ExtractBits(gen.RandUint64(), from_bit, num_bits);
    samples[sample] += 1;
  }

  // Compute the Chi-Squared statistic, which is:
  // \Sum_{k=0}^{range-1} \frac{(count - expected)^2}{expected}
  double chi_squared = 0.;
  double expected_count = n / range;
  for (size_t sample_count : samples) {
    double deviation = sample_count - expected_count;
    chi_squared += (deviation * deviation) / expected_count;
  }

  // The generator should produce numbers that are not too far of (chi_squared
  // lower than a given quantile), but not too close to the ideal distribution
  // either (chi_squared is too low).
  //
  // See The Art Of Computer Programming, Volume 2, Section 3.3.1 for details.
  return chi_squared > ChiSquaredCriticalValue(range - 1, kXp1Percent) &&
         chi_squared < ChiSquaredCriticalValue(range - 1, kXp99Percent);
}

}  // namespace

TEST(RandUtilTest, InsecureRandomGeneratorChiSquared) {
  constexpr int kIterations = 50;

  // Specifically test the low bits, which are usually weaker in random number
  // generators. We don't use them for the 32 bit number generation, but let's
  // make sure they are still suitable.
  for (int start_bit : {1, 2, 3, 8, 12, 20, 32, 48, 54}) {
    int pass_count = 0;
    for (int i = 0; i < kIterations; i++) {
      size_t samples = 1 << 16;
      InsecureRandomGenerator gen;
      // Fix the seed to make the test non-flaky.
      gen.ReseedForTesting(kIterations + 1);
      bool pass = ChiSquaredTest(gen, samples, start_bit, 8);
      pass_count += pass;
    }

    // We exclude 1% on each side, so we expect 98% of tests to pass, meaning 98
    // * kIterations / 100. However this is asymptotic, so add a bit of leeway.
    int expected_pass_count = (kIterations * 98) / 100;
    EXPECT_GE(pass_count, expected_pass_count - ((kIterations * 2) / 100))
        << "For start_bit = " << start_bit;
  }
}

TEST(RandUtilTest, InsecureRandomGeneratorRandDouble) {
  InsecureRandomGenerator gen;

  for (int i = 0; i < 1000; i++) {
    volatile double x = gen.RandDouble();
    EXPECT_GE(x, 0.);
    EXPECT_LT(x, 1.);
  }
}

TEST(RandUtilTest, MetricsSubSampler) {
  MetricsSubSampler sub_sampler;
  int true_count = 0;
  int false_count = 0;
  for (int i = 0; i < 1000; ++i) {
    if (sub_sampler.ShouldSample(0.5)) {
      ++true_count;
    } else {
      ++false_count;
    }
  }

  // Validate that during normal operation MetricsSubSampler::ShouldSample()
  // does not always give the same result. It's technically possible to fail
  // this test during normal operation but if the sampling is realistic it
  // should happen about once every 2^999 times (the likelihood of the [1,999]
  // results being the same as [0], which can be either). This should not make
  // this test flaky in the eyes of automated testing.
  EXPECT_GT(true_count, 0);
  EXPECT_GT(false_count, 0);
}

TEST(RandUtilTest, MetricsSubSamplerTestingSupport) {
  MetricsSubSampler sub_sampler;

  // ScopedAlwaysSampleForTesting makes ShouldSample() return true with
  // any probability.
  {
    MetricsSubSampler::ScopedAlwaysSampleForTesting always_sample;
    for (int i = 0; i < 100; ++i) {
      EXPECT_TRUE(sub_sampler.ShouldSample(0));
      EXPECT_TRUE(sub_sampler.ShouldSample(0.5));
      EXPECT_TRUE(sub_sampler.ShouldSample(1));
    }
  }

  // ScopedNeverSampleForTesting makes ShouldSample() return true with
  // any probability.
  {
    MetricsSubSampler::ScopedNeverSampleForTesting always_sample;
    for (int i = 0; i < 100; ++i) {
      EXPECT_FALSE(sub_sampler.ShouldSample(0));
      EXPECT_FALSE(sub_sampler.ShouldSample(0.5));
      EXPECT_FALSE(sub_sampler.ShouldSample(1));
    }
  }
}

}  // namespace base
