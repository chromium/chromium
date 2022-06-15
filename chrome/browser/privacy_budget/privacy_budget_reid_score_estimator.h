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

  // Using this getter for unit testing to examine its values.
  const base::flat_map<blink::IdentifiableSurface, SurfacesAndOptionalValues>&
  GetSurfacesAndValuesForTesting();

  // Searches the storage for the surface.
  // If found, it updates its value to the token sent.
  void ProcessForReidScore(blink::IdentifiableSurface surface,
                           blink::IdentifiableToken token);

 private:
  // Keeps track of the set of surfaces for which the Reid scorre is calculated.
  base::flat_map<blink::IdentifiableSurface, SurfacesAndOptionalValues>
      surfaces_and_values_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_
