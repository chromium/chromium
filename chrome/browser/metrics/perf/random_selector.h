// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_RANDOM_SELECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_RANDOM_SELECTOR_H_

#include <stddef.h>

#include <string>
#include <vector>

// RandomSelector can be used to pick vectors of strings according to certain
// probabilities. The probabilities are set using SetOdds(). A randomly picked
// vector can be obtained by calling Select().
//
// Sample usage:
//
// RandomSelector random_selector;
// std::vector<RandomSelector::WeightAndValue> odds {
//   {50, "a"},
//   {40, "b"},
//   {10, "c"}
// };
// random_selector.SetOdds(odds);
//
// std::vector<std::string>& selection = random_selector.Select();
//
// The above line should return "a" with a probability of 50%,
// "b" with a probability of 40%, and "c" with a probability of 10%:
class RandomSelector {
 public:
  struct WeightAndValue {
    WeightAndValue(double weight, const std::string& value)
      : weight(weight), value(value) {
    }

    bool operator==(const WeightAndValue& other) const {
      return weight == other.weight && value == other.value;
    }

    // Probability weight for selecting this value.
    double weight;
    // Value to be returned by Select(), if selected.
    std::string value;
  };

  RandomSelector();

  RandomSelector(const RandomSelector&) = delete;
  RandomSelector& operator=(const RandomSelector&) = delete;

  virtual ~RandomSelector();

  // Set the probabilities for various strings. Returns false and doesn't
  // modify the values if odds contains any invalid weights (<=0.0) or if
  // odds is empty.
  bool SetOdds(const std::vector<WeightAndValue>& odds);

  // Randomly select one of the values from the set.
  const std::string& Select();

  // Returns the number of string entries.
  size_t num_values() const {
    return odds_.size();
  }

  const std::vector<WeightAndValue>& odds() const { return odds_; }

  // Sum of the |weight| fields in the vector. Returns -1.0 if odds contains any
  // weight <= 0.0.
  static double SumWeights(const std::vector<WeightAndValue>& odds);

 private:
  // Get a floating point number between 0.0 and |max|.
  virtual double RandDoubleUpTo(double max);

  // Get a string corresponding to |random| that is in the odds vector.
  // |random| must be a number between zero and the sum of the probability
  // weights.
  const std::string& GetValueFor(double random);

  // A dictionary representing the strings to choose from and associated odds.
  std::vector<WeightAndValue> odds_;

  // Sum of the probability weights.
  double sum_of_weights_;
};

::std::ostream& operator<<(
    ::std::ostream& os, const RandomSelector::WeightAndValue& value);

#endif  // CHROME_BROWSER_METRICS_PERF_RANDOM_SELECTOR_H_
