// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_REID_SCORE_ESTIMATOR_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_REID_SCORE_ESTIMATOR_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/privacy_budget/identifiability_study_group_settings.h"
#include "chrome/common/privacy_budget/types.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"

// Temporary surface-value storage to estimate the Reid score of specified
// surface set.
class PrivacyBudgetReidScoreEstimator {
 public:
  explicit PrivacyBudgetReidScoreEstimator(
      const IdentifiabilityStudyGroupSettings& state_settings);

  PrivacyBudgetReidScoreEstimator(const PrivacyBudgetReidScoreEstimator&) =
      delete;
  PrivacyBudgetReidScoreEstimator& operator=(
      const PrivacyBudgetReidScoreEstimator&) = delete;

  ~PrivacyBudgetReidScoreEstimator();

  // Searches the storage for the surface.
  // If found, it updates its value to the token sent.
  void ProcessForReidScore(blink::IdentifiableSurface surface,
                           blink::IdentifiableToken token);

 private:
  // Keeps track of the set of surfaces for which the Reid scorre is calculated.
  base::flat_map<blink::IdentifiableSurface, SurfacesAndOptionalValues>
      surfaces_and_values_;

  // Keeps track of the set of max of salt ranges to calculate the Reid hash for
  // every surface block.
  std::vector<uint64_t> reid_blocks_salts_ranges_;

  // Keeps track of the number of bits that should be reported for every Reid
  // surface block.
  std::vector<int> reid_blocks_bits_;

  // Keeps track of the probability of noise that should be reported for every
  // Reid surface block.
  std::vector<double> reid_blocks_noise_probabilities_;

  // Keeps track of the number of reported surfaces in every Reid surface block.
  // The Reid surface map at index i is full when the count_flag_ at i is equal
  // to the number of surfaces in that map i.e. size of the map.
  std::vector<uint64_t> count_flag_;

  // Compute the hash for estimating the REID score.
  uint64_t ComputeHashForReidScore(const SurfacesAndOptionalValues& surface_map,
                                   uint64_t max_num_salt,
                                   int reid_bits,
                                   double reid_noise_probability);
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_
