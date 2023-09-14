// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_UTILS_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_UTILS_H_

namespace tpcd::experiment::utils {

enum class ExperimentCohort {
  kUnset = 0,
  kIneligible = 1,
  kModeB = 2,
  kModeBPrime = 3,
  kControl = 4,
  kControlPrime = 5,
  kMaxValue = kControlPrime,
};

enum class ExperimentState {
  kUnknownEligiblity = 0,
  kIneligible = 1,
  kEligible = 2,
  kOnboardedEligible = 3,
  kMaxValue = kOnboardedEligible,
};

}  // namespace tpcd::experiment::utils

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_UTILS_H_
