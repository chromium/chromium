// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/random_selector.h"

#include <stddef.h>

#include <cmath>
#include <map>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

// A small floating point number used to verify that the expected odds are equal
// to the odds set.
const double kEpsilon = 0.01;

// A class that overrides RandDoubleUpTo() to not be random. The number
// generator will emulate a uniform distribution of numbers between 0.0 and
// |max| when called with the same |max| parameter and a whole multiple of
// |random_period| times. This allows better testing of the RandomSelector
// class.
class RandomSelectorWithCustomRNG : public RandomSelector {
 public:
  explicit RandomSelectorWithCustomRNG(unsigned int random_period)
      : random_period_(random_period), current_index_(0) {}

  RandomSelectorWithCustomRNG(const RandomSelectorWithCustomRNG&) = delete;
  RandomSelectorWithCustomRNG& operator=(const RandomSelectorWithCustomRNG&) =
      delete;

 private:
  // This function returns floats between 0.0 and |max| in an increasing
  // fashion at regular intervals.
  double RandDoubleUpTo(double max) override {
    current_index_ = (current_index_ + 1) % random_period_;
    return max * current_index_ / random_period_;
  }

  // Period (number of calls) over which the fake RNG repeats.
  const unsigned int random_period_;

  // Stores the current position we are at in the interval between 0.0 and
  // |max|. See the function RandDoubleUpTo for details on how this is used.
  int current_index_;
};

// Use the random_selector to generate some values. The number of values to
// generate is |iterations|.
void GenerateResults(size_t iterations,
                     RandomSelector* random_selector,
                     std::map<std::string, int>* results) {
  for (size_t i = 0; i < iterations; ++i) {
    const std::string& next_value = random_selector->Select();
    (*results)[next_value]++;
  }
}

// This function tests whether the results are close enough to the odds (within
// 1%).
void CheckResultsAgainstOdds(
    const std::vector<RandomSelector::WeightAndValue>& odds,
    const std::map<std::string, int>& results) {
  EXPECT_EQ(odds.size(), results.size());

  const double odds_sum = RandomSelector::SumWeights(odds);
  int results_sum = 0;
  for (const auto& item : results) {
    results_sum += item.second;
  }

  for (const auto& odd : odds) {
    const auto result = results.find(odd.value);
    EXPECT_NE(result, results.end());
    const double results_ratio = 1.0*result->second / results_sum;
    const double odds_ratio = odd.weight / odds_sum;
    const double abs_diff = std::abs(results_ratio - odds_ratio);
    EXPECT_LT(abs_diff, kEpsilon);
  }
}

TEST(RandomSelector, SimpleAccessors) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<WeightAndValue> odds;
  odds.push_back(WeightAndValue(1, "a 1"));
  odds.push_back(WeightAndValue(3, "b --help"));
  odds.push_back(WeightAndValue(107, "c bar"));
  EXPECT_EQ(111.0L, RandomSelector::SumWeights(odds));
  RandomSelector random_selector;
  EXPECT_TRUE(random_selector.SetOdds(odds));
  EXPECT_EQ(3UL, random_selector.num_values());
  EXPECT_EQ(odds, random_selector.odds());
}

// Ensure RandomSelector is able to generate results from given odds.
TEST(RandomSelector, GenerateTest) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  const int kLargeNumber = 2000;
  std::vector<RandomSelector::WeightAndValue> odds;
  odds.push_back(WeightAndValue(1, "a 1"));
  odds.push_back(WeightAndValue(2, "b --help"));
  odds.push_back(WeightAndValue(3, "c bar"));
  RandomSelectorWithCustomRNG random_selector(kLargeNumber);
  EXPECT_TRUE(random_selector.SetOdds(odds));
  // Generate a lot of values.
  std::map<std::string, int> results;
  GenerateResults(kLargeNumber, &random_selector, &results);
  // Ensure the values and odds are related.
  CheckResultsAgainstOdds(odds, results);
}

TEST(RandomSelector, InvalidWeights) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<RandomSelector::WeightAndValue> good_odds;
  good_odds.push_back(WeightAndValue(1, "a 1"));
  good_odds.push_back(WeightAndValue(2, "b --help"));
  good_odds.push_back(WeightAndValue(3, "c bar"));
  RandomSelector random_selector;
  EXPECT_TRUE(random_selector.SetOdds(good_odds));
  EXPECT_EQ(good_odds, random_selector.odds());

  std::vector<RandomSelector::WeightAndValue> bad_odds;
  bad_odds.push_back(WeightAndValue(1, "a 1"));
  bad_odds.push_back(WeightAndValue(2, "b --help"));
  bad_odds.push_back(WeightAndValue(-3.5, "c bar"));
  EXPECT_FALSE(random_selector.SetOdds(bad_odds));
  EXPECT_EQ(good_odds, random_selector.odds());

  bad_odds[2].weight = 0.0;
  EXPECT_FALSE(random_selector.SetOdds(bad_odds));
  EXPECT_EQ(good_odds, random_selector.odds());
}

TEST(RandomSelector, EmptyWeights) {
  using WeightAndValue = RandomSelector::WeightAndValue;
  std::vector<RandomSelector::WeightAndValue> good_odds;
  good_odds.push_back(WeightAndValue(1, "a 1"));
  good_odds.push_back(WeightAndValue(2, "b --help"));
  good_odds.push_back(WeightAndValue(3, "c bar"));
  RandomSelector random_selector;
  EXPECT_TRUE(random_selector.SetOdds(good_odds));
  EXPECT_EQ(good_odds, random_selector.odds());

  std::vector<RandomSelector::WeightAndValue> empty_odds;
  EXPECT_FALSE(random_selector.SetOdds(empty_odds));
  EXPECT_EQ(good_odds, random_selector.odds());
}
