// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/revisit_count_revisit_estimator.h"

namespace performance_manager {

float RevisitCountRevisitEstimator::ComputeRevisitProbability(
    const TabPageDecorator::TabHandle* tab_handle) {
  // TODO(crbug.com/1469337): Compute the actual probability. 1 means the tab is
  // as likely to be revisited as possible, so it won't be discarded.
  return 1.0f;
}

}  // namespace performance_manager
