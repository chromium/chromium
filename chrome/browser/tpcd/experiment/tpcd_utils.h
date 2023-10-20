// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_UTILS_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_UTILS_H_

namespace tpcd::experiment::utils {

enum class ExperimentState {
  kUnknownEligibility = 0,
  kIneligible = 1,
  kEligible = 2,
  kMaxValue = kEligible,
};

enum class Experiment3PCBlockStatus {
  kAllowedAndExperimentBlocked = 0,
  kAllowedAndExperimentAllowed = 1,
  kBlockedAndExperimentBlocked = 2,
  kBlockedAndExperimentAllowed = 3,
  kMaxValue = kBlockedAndExperimentAllowed,
};

const char Experiment3pcBlockStatusHistogramName[] =
    "Privacy.3pcd.Experiment3pcBlockStatus";

}  // namespace tpcd::experiment::utils

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_UTILS_H_
