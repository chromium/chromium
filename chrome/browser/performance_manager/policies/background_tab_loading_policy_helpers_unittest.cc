// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy_helpers.h"

#include <math.h>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

namespace {

class BackgroundTabLoadingPolicyHelpersTest : public ::testing::Test {};

}  // namespace

TEST_F(BackgroundTabLoadingPolicyHelpersTest,
       CalculateMaxSimultaneousTabLoads) {
  // Test the lower bound is enforced.
  EXPECT_EQ(10u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     1 /* cores_per_load */, 1 /* cores */));

  // Test the upper bound is enforced.
  EXPECT_EQ(20u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     1 /* cores_per_load */, 30 /* cores */));

  // Test the per-core calculation is correct.
  EXPECT_EQ(15u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     1 /* cores_per_load */, 15 /* cores */));
  EXPECT_EQ(15u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     2 /* cores_per_load */, 30 /* cores */));

  // If no per-core is specified then upper_bound is returned.
  EXPECT_EQ(5u, CalculateMaxSimultaneousTabLoads(
                    1 /* lower_bound */, 5 /* upper_bound */,
                    0 /* cores_per_load */, 10 /* cores */));

  // If no per-core and no upper_bound is applied, then "upper_bound" is
  // returned.
  EXPECT_EQ(
      std::numeric_limits<size_t>::max(),
      CalculateMaxSimultaneousTabLoads(3 /* lower_bound */, 0 /* upper_bound */,
                                       0 /* cores_per_load */, 4 /* cores */));
}

TEST_F(BackgroundTabLoadingPolicyHelpersTest, CalculateAgeScore) {
  // Generate a bunch of random tab age data.
  std::vector<std::pair<base::TimeDelta, float>> tab_age_score;
  tab_age_score.reserve(1000);

  // Generate some known edge cases.
  tab_age_score.push_back(std::make_pair(base::Milliseconds(-1001), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(-1000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(-999), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(-500), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(0), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(500), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(999), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(1000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Milliseconds(1001), 0.0));

  // Generate a logarithmic selection of ages to test the whole range.
  tab_age_score.push_back(std::make_pair(base::Seconds(-1000000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(-100000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(-10000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(-1000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(-100), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(-10), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(10), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(100), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(1000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(10000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(100000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(1000000), 0.0));
  tab_age_score.push_back(std::make_pair(base::Seconds(10000000), 0.0));

  constexpr int kMonthInSeconds = 60 * 60 * 24 * 31;

  // Generate a bunch more random ages.
  for (size_t i = tab_age_score.size(); i < 1000; ++i) {
    tab_age_score.push_back(std::make_pair(
        base::Seconds(base::RandInt(-kMonthInSeconds, kMonthInSeconds)), 0.0));
  }

  // Calculate the tab scores.
  for (auto age_score : tab_age_score) {
    age_score.second = CalculateAgeScore(age_score.first.InSecondsF());
  }

  // Sort tab scores by increasing last active time.
  std::sort(tab_age_score.begin(), tab_age_score.end(),
            [](const std::pair<base::TimeDelta, float>& tab1,
               const std::pair<base::TimeDelta, float>& tab2) {
              return tab1.first < tab2.first;
            });

  // The scores should be in decreasing order (>= is necessary because some
  // last active times collapse to the same score).
  for (size_t i = 1; i < tab_age_score.size(); ++i)
    ASSERT_GE(tab_age_score[i - 1].second, tab_age_score[i].second);
}

}  // namespace policies

}  // namespace performance_manager
