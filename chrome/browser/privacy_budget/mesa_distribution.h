// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_MESA_DISTRIBUTION_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_MESA_DISTRIBUTION_H_

#include <cmath>
#include <random>
#include <set>
#include <type_traits>
#include "base/check_op.h"

// Generates a set of integers drawn from a mesa shaped probability distribution
// with replacement.
//
// The PDF is:
//
//            ⎧    0                               ... if x < 0
//            ⎪
//     P(x) = ⎨    λ                               ... if 0 <= x < T
//            ⎪
//            ⎩    (1 - τ) * γ * (1 - γ)^{X - T}   ... otherwise
//
// where
//
//   T = Value at which the PDF switches from a uniform to a geometric
//       distribution. Referred to in code as the `pivot_point`.
//
//   τ = Ratio of probability between linear region of the PDF. I.e. if τ = 0.9,
//       then 90% of the probability space is in the linear region. The ratio is
//       referred to in code as `dist_ratio`.
//
//   γ = Parameter of the geometric distribution.
//
//        τ
//   λ = ───
//        T
//
// In otherwords, the PDF is uniform up to T with a probability of λ, and then
// switches to a geometric distribution with parameter γ that extends to
// infinity.
//
// It looks like this in the form of a graph which should make a little bit more
// sense.
//
//          P(x)   ▲
//                 │
//   probability  λ│┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┬,
//   density       │    uniform     ┊ L        geometric
//                 │  distribution  ┊  "._    distribution
//                 │                ┊     `--..______
//                 └────────────────┴──────────────────▶ x
//                 0                T
//
// Why this odd combination of disjoint probability distributions?
//
// Such a distribution is useful when you want to select some set of elements
// uniformly up to a threshold, but want to allow for a tail distribution that
// extends arbitrarily past that range.
//
// The τ parameter establishes the balance between the linear region and the
// geometric region, while T establishes the scale. Typically we set τ to
// something close to 0.9 or so such that 0.1 of the probability space is
// reserved for the long tail.
//
// Parameters:
//   pivot_point: T as described above. Any value bigger than 0.
//   dist_ratio : τ as described above. Must be in (0,1).
template <typename ResultType,
          std::enable_if_t<std::is_integral<ResultType>::value, int> = 0>
class MesaDistribution {
 public:
  MesaDistribution(ResultType pivot_point,
                   double dist_ratio,
                   double geometric_distribution_param)
      : pivot_point_(pivot_point),
        uniform_distribution_(0, std::ceil(pivot_point / dist_ratio)),
        geometric_distribution_(geometric_distribution_param) {
    DCHECK_GT(pivot_point, static_cast<ResultType>(0));
    DCHECK_GT(dist_ratio, 0);
    DCHECK_LT(dist_ratio, 1);
  }

  ~MesaDistribution() = default;

  // Draws a single value from the distribution.
  //
  // `Generator` must satisfy `UniformRandomBitGenerator`.
  // https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator
  template <class Generator>
  ResultType Get(Generator& g) {
    ResultType v = uniform_distribution_(g);
    if (v >= pivot_point_)
      return pivot_point_ + geometric_distribution_(g);
    return v;
  }

  // RandomNumberDistribution.
  // https://en.cppreference.com/w/cpp/named_req/RandomNumberDistribution
  //
  // `Generator` must satisfy `UniformRandomBitGenerator`.
  // https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator
  template <class Generator>
  ResultType operator()(Generator g) {
    return Get(g);
  }

  ResultType pivot_point() const { return pivot_point_; }

 private:
  const ResultType pivot_point_;
  std::uniform_int_distribution<ResultType> uniform_distribution_;
  std::geometric_distribution<ResultType> geometric_distribution_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_MESA_DISTRIBUTION_H_
