// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/random_selector.h"

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

RandomSelector::RandomSelector() : sum_of_weights_(0) {}

RandomSelector::~RandomSelector() {}

// static
double RandomSelector::SumWeights(const std::vector<WeightAndValue>& odds) {
  double sum = 0.0;
  for (const auto& odd : odds) {
    if (odd.weight <= 0.0)
      return -1.0;
    sum += odd.weight;
  }
  return sum;
}

bool RandomSelector::SetOdds(const std::vector<WeightAndValue>& odds) {
  double sum = SumWeights(odds);
  if (sum <= 0.0)
    return false;
  odds_ = odds;
  sum_of_weights_ = sum;
  return true;
}

const std::string& RandomSelector::Select() {
  // Get a random double between 0 and the sum.
  double random = RandDoubleUpTo(sum_of_weights_);
  // Figure out what it belongs to.
  return GetValueFor(random);
}

double RandomSelector::RandDoubleUpTo(double max) {
  CHECK_GT(max, 0.0);
  return max * base::RandDouble();
}

const std::string& RandomSelector::GetValueFor(double random) {
  double current = 0.0;
  for (const auto& odd : odds_) {
    current += odd.weight;
    if (random < current)
      return odd.value;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid value for key: " << random;
  return base::EmptyString();
}

// Print the value. Used for friendly test failure messages.
::std::ostream& operator<<(
    ::std::ostream& os, const RandomSelector::WeightAndValue& value) {
  return os << "{" << value.weight << ", \"" << value.value << "\"}";
}
