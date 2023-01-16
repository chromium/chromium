// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/mesa_distribution.h"
#include <math.h>

#include <array>
#include <limits>
#include <random>
#include <set>

#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr auto kSeed = 3;
constexpr auto kPivotPoint = 300;
constexpr auto kDistRatio = 0.9l;
constexpr auto kGeometricDistributionParam = 0.5l;
}  // namespace

TEST(MesaDistributionTest, Get) {
  MesaDistribution<int> mesa(kPivotPoint, kDistRatio,
                             kGeometricDistributionParam);
  std::mt19937 g(kSeed);
  auto v1 = mesa.Get(g);
  g.seed(kSeed);
  auto v2 = mesa.Get(g);
  EXPECT_EQ(v1, v2);
}

// This test asserts that the MesaDistribution produces a near ideal
// distribution of offset selections.
//
// We do this by drawing a very large number of samples from MesaDistribution in
// the presence of a fairly good PRNG and verifying that the resulting
// probability distribution is within a close margin of the ideal distribution.
//
// In order to pass the test, the simulation must result in an aggregate
// probability distribution that's within ε of the ideal distribution.
//
// The PRNG is MT19937 with a fixed seed.
TEST(MesaDistributionTest, SpreadTest_MaybeSlow) {
  constexpr auto kTrials = 10'000'000lu;

  // We truncate the distribution at kMaxOffset, or else we'd need to keep
  // occurrence counts for over a very large range.
  //
  // Truncation changes the probability distribution as a side-effect. Observed
  // probability density increases by a factor of 1 / CDF(kMaxOffset) where CDF
  // is the cumulative distribution function for Mesa. However beyond
  // 2 * kPivotPoint the CDF is incalculably close to 1.
  constexpr auto kMaxOffset = kPivotPoint * 3;

  // Lambda corresponds to λ in the description in mesa_distribution.h
  // explaining the distribution. It's the probability density within the linear
  // region of the distribution.
  const auto kLambda = kDistRatio / kPivotPoint;

  // Gamma corresponds to γ in the description in mesa_distribution.h. It's the
  // parameter to the Geometric distribution in the tail region.
  const auto kGamma = kLambda / (1 - kDistRatio);

  // kEpsilon is the maximum absolute error in probability per element. We
  // expect the experiment below to produce values within 5% of the linear
  // region density.
  const double kEpsilon = kLambda * 0.05 /* ±5% envelope */;

  // occurrence[i] := the number of times we've seen `i`.
  std::array<double, kMaxOffset + 1> occurrences = {0.0};

  // The distribution under test:
  MesaDistribution<int> mesa(kPivotPoint, kDistRatio, kGamma);

  std::mt19937 random_bit_generator(kSeed);

  for (auto i = 0u; i < kTrials; ++i) {
    auto v = mesa.Get(random_bit_generator);
    if (v > kMaxOffset)
      continue;
    ++occurrences[v];
  }

  // `occurrences` is a histogram of seen values. This loop converts it to
  // a probability.
  for (double& occurrence : occurrences)
    occurrence /= kTrials;

  // Offsets from 0 thru `kPivotPoint - 1` (inclusive) should have the same
  // probability. I.e. uniform.
  for (int i = 0; i < kPivotPoint; ++i) {
    ASSERT_NEAR(occurrences[i], kLambda, kEpsilon) << "at offset" << i;
  }

  // Offsets from `kPivotPoint` thru infinity should follow a geometric
  // distribution. We compare the observed probabilities with the Geometric PDF.
  double expected_pdf = kLambda;
  for (int i = kPivotPoint; i <= kMaxOffset; ++i) {
    ASSERT_NEAR(occurrences[i], expected_pdf, kEpsilon) << "at offset" << i;
    expected_pdf *= 1.0l - kGamma;
  }
}
