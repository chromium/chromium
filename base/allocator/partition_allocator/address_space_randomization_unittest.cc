// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_space_randomization.h"

#include <vector>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/random.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/windows_version.h"
// VersionHelpers.h must be included after windows.h.
#include <VersionHelpers.h>
#endif

namespace base {

namespace {

uintptr_t GetMask() {
  uintptr_t mask = internal::kASLRMask;
#if defined(ARCH_CPU_64_BITS)
// Sanitizers use their own kASLRMask constant.
#if defined(OS_WIN) && !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  if (!IsWindows8Point1OrGreater()) {
    mask = internal::kASLRMaskBefore8_10;
  }
#endif  // defined(OS_WIN) && !defined(MEMORY_TOOL_REPLACES_ALLOCATOR))
#elif defined(ARCH_CPU_32_BITS)
#if defined(OS_WIN)
  BOOL is_wow64 = FALSE;
  if (!IsWow64Process(GetCurrentProcess(), &is_wow64))
    is_wow64 = FALSE;
  if (!is_wow64) {
    mask = 0;
  }
#endif  // defined(OS_WIN)
#endif  // defined(ARCH_CPU_32_BITS)
  return mask;
}

const size_t kSamples = 100;

uintptr_t GetAddressBits() {
  return reinterpret_cast<uintptr_t>(base::GetRandomPageBase());
}

uintptr_t GetRandomBits() {
  return GetAddressBits() - internal::kASLROffset;
}

}  // namespace

// Configurations without ASLR are tested here.
TEST(AddressSpaceRandomizationTest, DisabledASLR) {
  uintptr_t mask = GetMask();
  if (!mask) {
#if defined(OS_WIN) && defined(ARCH_CPU_32_BITS)
    // ASLR should be turned off on 32-bit Windows.
    EXPECT_EQ(nullptr, base::GetRandomPageBase());
#else
    // Otherwise, nullptr is very unexpected.
    EXPECT_NE(nullptr, base::GetRandomPageBase());
#endif
  }
}

TEST(AddressSpaceRandomizationTest, Alignment) {
  uintptr_t mask = GetMask();
  if (!mask)
    return;

  for (size_t i = 0; i < kSamples; ++i) {
    uintptr_t address = GetAddressBits();
    EXPECT_EQ(0ULL, (address & kPageAllocationGranularityOffsetMask));
  }
}

TEST(AddressSpaceRandomizationTest, Range) {
  uintptr_t mask = GetMask();
  if (!mask)
    return;

  uintptr_t min = internal::kASLROffset;
  uintptr_t max = internal::kASLROffset + internal::kASLRMask;
  for (size_t i = 0; i < kSamples; ++i) {
    uintptr_t address = GetAddressBits();
    EXPECT_LE(min, address);
    EXPECT_GE(max + mask, address);
  }
}

TEST(AddressSpaceRandomizationTest, Predictable) {
  uintptr_t mask = GetMask();
  if (!mask)
    return;

  const uint64_t kInitialSeed = 0xfeed5eedULL;
  base::SetMmapSeedForTesting(kInitialSeed);

  std::vector<uintptr_t> sequence;
  for (size_t i = 0; i < kSamples; ++i) {
    uintptr_t address = reinterpret_cast<uintptr_t>(base::GetRandomPageBase());
    sequence.push_back(address);
  }

  base::SetMmapSeedForTesting(kInitialSeed);

  for (size_t i = 0; i < kSamples; ++i) {
    uintptr_t address = reinterpret_cast<uintptr_t>(base::GetRandomPageBase());
    EXPECT_EQ(address, sequence[i]);
  }
}

// This randomness test is adapted from V8's PRNG tests.

// Chi squared for getting m 0s out of n bits.
double ChiSquared(int m, int n) {
  double ys_minus_np1 = (m - n / 2.0);
  double chi_squared_1 = ys_minus_np1 * ys_minus_np1 * 2.0 / n;
  double ys_minus_np2 = ((n - m) - n / 2.0);
  double chi_squared_2 = ys_minus_np2 * ys_minus_np2 * 2.0 / n;
  return chi_squared_1 + chi_squared_2;
}

// Test for correlations between recent bits from the PRNG, or bits that are
// biased.
void RandomBitCorrelation(int random_bit) {
  uintptr_t mask = GetMask();
  if ((mask & (1ULL << random_bit)) == 0)
    return;  // bit is always 0.

#ifdef DEBUG
  constexpr int kHistory = 2;
  constexpr int kRepeats = 1000;
#else
  constexpr int kHistory = 8;
  constexpr int kRepeats = 10000;
#endif
  constexpr int kPointerBits = 8 * sizeof(void*);
  uintptr_t history[kHistory];
  // The predictor bit is either constant 0 or 1, or one of the bits from the
  // history.
  for (int predictor_bit = -2; predictor_bit < kPointerBits; predictor_bit++) {
    // The predicted bit is one of the bits from the PRNG.
    for (int ago = 0; ago < kHistory; ago++) {
      // We don't want to check whether each bit predicts itself.
      if (ago == 0 && predictor_bit == random_bit)
        continue;

      // Enter the new random value into the history.
      for (int i = ago; i >= 0; i--) {
        history[i] = GetRandomBits();
      }

      // Find out how many of the bits are the same as the prediction bit.
      int m = 0;
      for (int i = 0; i < kRepeats; i++) {
        uintptr_t random = GetRandomBits();
        for (int j = ago - 1; j >= 0; j--)
          history[j + 1] = history[j];
        history[0] = random;

        int predicted;
        if (predictor_bit >= 0) {
          predicted = (history[ago] >> predictor_bit) & 1;
        } else {
          predicted = predictor_bit == -2 ? 0 : 1;
        }
        int bit = (random >> random_bit) & 1;
        if (bit == predicted)
          m++;
      }

      // Chi squared analysis for k = 2 (2, states: same/not-same) and one
      // degree of freedom (k - 1).
      double chi_squared = ChiSquared(m, kRepeats);
      // For k=2 probability of Chi^2 < 35 is p=3.338e-9. This condition is
      // tested ~19000 times, so probability of it failing randomly per one
      // base_unittests run is (1 - (1 - p) ^ 19000) ~= 6e-5.
      CHECK_LE(chi_squared, 35.0);
      // If the predictor bit is a fixed 0 or 1 then it makes no sense to
      // repeat the test with a different age.
      if (predictor_bit < 0)
        break;
    }
  }
}

// Tests are fairly slow, so give each random bit its own test.
#define TEST_RANDOM_BIT(BIT)                                        \
  TEST(AddressSpaceRandomizationTest, RandomBitCorrelations##BIT) { \
    RandomBitCorrelation(BIT);                                      \
  }

// The first 12 bits on all platforms are always 0.
TEST_RANDOM_BIT(12)
TEST_RANDOM_BIT(13)
TEST_RANDOM_BIT(14)
TEST_RANDOM_BIT(15)
TEST_RANDOM_BIT(16)
TEST_RANDOM_BIT(17)
TEST_RANDOM_BIT(18)
TEST_RANDOM_BIT(19)
TEST_RANDOM_BIT(20)
TEST_RANDOM_BIT(21)
TEST_RANDOM_BIT(22)
TEST_RANDOM_BIT(23)
TEST_RANDOM_BIT(24)
TEST_RANDOM_BIT(25)
TEST_RANDOM_BIT(26)
TEST_RANDOM_BIT(27)
TEST_RANDOM_BIT(28)
TEST_RANDOM_BIT(29)
TEST_RANDOM_BIT(30)
TEST_RANDOM_BIT(31)
#if defined(ARCH_CPU_64_BITS)
TEST_RANDOM_BIT(32)
TEST_RANDOM_BIT(33)
TEST_RANDOM_BIT(34)
TEST_RANDOM_BIT(35)
TEST_RANDOM_BIT(36)
TEST_RANDOM_BIT(37)
TEST_RANDOM_BIT(38)
TEST_RANDOM_BIT(39)
TEST_RANDOM_BIT(40)
TEST_RANDOM_BIT(41)
TEST_RANDOM_BIT(42)
TEST_RANDOM_BIT(43)
TEST_RANDOM_BIT(44)
TEST_RANDOM_BIT(45)
TEST_RANDOM_BIT(46)
TEST_RANDOM_BIT(47)
TEST_RANDOM_BIT(48)
// No platforms have more than 48 address bits.
#endif  // defined(ARCH_CPU_64_BITS)

#undef TEST_RANDOM_BIT

}  // namespace base
