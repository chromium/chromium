// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_PROBABILITY_DISTRIBUTIONS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_PROBABILITY_DISTRIBUTIONS_H_

#include <map>

#include "chrome/browser/performance_manager/policies/probability_distribution.h"

namespace performance_manager {

// Returns a map of revisit count -> cumulative distribution function of the
// time to revisit a background tab. There should be
// `TabRevisitTracker::kMaxNumRevisit` entries in the map.
std::map<int64_t, ProbabilityDistribution>
CreatePerRevisitCountTimeToRevisitCdfs();

// Returns a map of revisit count -> probability of tab being revisited. There
// should be `TabRevisitTracker::kMaxNumRevisit` entries in the map.
std::map<int64_t, float> CreatePerRevisitCountRevisitProbability();

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_PROBABILITY_DISTRIBUTIONS_H_
