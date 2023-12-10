// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/revisit_probability_distributions.h"

namespace performance_manager {

std::map<int64_t, ProbabilityDistribution>
CreatePerRevisitCountTimeToRevisitCdfs() {
  // TODO(crbug.com/1469337): Compute these distributions from UMA data and
  // include the values here. Returning the empty map is safe because the
  // estimators will consider the probability of revisit as 1.0 if the
  // distributions needed for the computation don't exist.
  return {};
}

std::map<int64_t, float> CreatePerRevisitCountRevisitProbability() {
  // TODO(crbug.com/1469337): Compute these distributions from UMA data and
  // include the values here. Returning the empty map is safe because the
  // estimators will consider the probability of revisit as 1.0 if the
  // distributions needed for the computation don't exist.
  return {};
}

}  // namespace performance_manager
