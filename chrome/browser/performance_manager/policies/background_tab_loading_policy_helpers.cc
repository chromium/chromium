// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/background_tab_loading_policy_helpers.h"

#include <math.h>

#include <algorithm>
#include <limits>

#include "base/check_op.h"

namespace performance_manager {

namespace policies {

size_t CalculateMaxSimultaneousTabLoads(size_t lower_bound,
                                        size_t upper_bound,
                                        size_t cores_per_load,
                                        size_t num_cores) {
  DCHECK(upper_bound == 0 || lower_bound <= upper_bound);
  DCHECK(num_cores > 0);

  size_t loads = 0;

  // Setting |cores_per_load| == 0 means that no per-core limit is applied.
  if (cores_per_load == 0) {
    loads = std::numeric_limits<size_t>::max();
  } else {
    loads = num_cores / cores_per_load;
  }

  // If |upper_bound| isn't zero then apply the maximum that it implies.
  if (upper_bound != 0)
    loads = std::min(loads, upper_bound);

  loads = std::max(loads, lower_bound);

  return loads;
}

float CalculateAgeScore(double last_visibility_change_seconds) {
  // TODO(crbug.com/40121561): Determine via an experiment whether tabs could
  // simply be sorted by descending order of last visibility, instead of using
  // an opaque score.

  // Cap absolute values less than 1 so that the inverse will be between -1
  // and 1.
  double score = last_visibility_change_seconds;
  if (fabs(score) < 1.0f) {
    if (score > 0)
      score = 1;
    else
      score = -1;
  }
  DCHECK_LE(1.0f, fabs(score));

  // Invert the score (1 / score).
  // Really old (infinity) maps to 0 (lowest priority).
  // Really young positive age (1) maps to 1 (moderate priority).
  // A little in the future (-1) maps to -1 (moderate priority).
  // Really far in the future (-infinity) maps to 0 (highest priority).
  // Shifting negative scores from [-1, 0] to [1, 2] keeps the scores increasing
  // with priority.
  if (score < 0) {
    score = 2.0 + 1.0 / score;
  } else {
    score = 1.0 / score;
  }
  DCHECK_LE(0.0, score);
  DCHECK_GE(2.0, score);

  // Rescale the age score to the range [0, 1] so that it can be added to the
  // category scores already calculated. Divide by 2 + epsilon so that no
  // score will end up rounding up to 1.0, but instead be capped at 0.999.
  score /= 2.002;
  DCHECK_LE(0.0, score);
  DCHECK_GT(1.0, score);

  return score;
}

}  // namespace policies

}  // namespace performance_manager
