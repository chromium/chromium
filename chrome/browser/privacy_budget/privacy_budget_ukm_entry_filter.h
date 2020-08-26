// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_

#include <cstdint>
#include <memory>

#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "components/ukm/ukm_entry_filter.h"

// A ukm::UkmEntryFilter that enforces the current identifiability study state.
//
// Doesn't affect any other kind of UKM event other than Identifiability events.
class PrivacyBudgetUkmEntryFilter : public ukm::UkmEntryFilter {
 public:
  // |settings| must outlive PrivacyBudgetUkmEntryFilter.
  explicit PrivacyBudgetUkmEntryFilter(IdentifiabilityStudyState* state);

  PrivacyBudgetUkmEntryFilter(const PrivacyBudgetUkmEntryFilter&) = delete;
  PrivacyBudgetUkmEntryFilter& operator=(const PrivacyBudgetUkmEntryFilter&) =
      delete;

  // ukm::UkmEntryFilter
  bool FilterEntry(
      ukm::mojom::UkmEntry* entry,
      base::flat_set<uint64_t>* removed_metric_hashes) const override;
  void OnStoreRecordingsInReport() const override;

 private:
  IdentifiabilityStudyState* const identifiability_study_state_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_
