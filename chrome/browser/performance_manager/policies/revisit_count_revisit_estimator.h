// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_COUNT_REVISIT_ESTIMATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_COUNT_REVISIT_ESTIMATOR_H_

#include "components/performance_manager/user_tuning/proactive_discard_evaluator.h"

namespace performance_manager {

class RevisitCountRevisitEstimator
    : public ProactiveDiscardEvaluator::RevisitProbabilityEstimator {
 public:
  float ComputeRevisitProbability(
      const TabPageDecorator::TabHandle* tab_handle) override;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_COUNT_REVISIT_ESTIMATOR_H_
