// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "components/ukm/ukm_entry_filter.h"

// A ukm::UkmEntryFilter that enforces the current identifiability study state.
//
// Doesn't affect any other kind of UKM event other than Identifiability events.
class PrivacyBudgetUkmEntryFilter : public ukm::UkmEntryFilter {
 public:
  // |state| must outlive PrivacyBudgetUkmEntryFilter.
  explicit PrivacyBudgetUkmEntryFilter(IdentifiabilityStudyState* state);

  PrivacyBudgetUkmEntryFilter(const PrivacyBudgetUkmEntryFilter&) = delete;
  PrivacyBudgetUkmEntryFilter& operator=(const PrivacyBudgetUkmEntryFilter&) =
      delete;

  // ukm::UkmEntryFilter
  bool FilterEntry(ukm::mojom::UkmEntry* entry,
                   base::flat_set<uint64_t>* removed_metric_hashes) final;
  void OnStoreRecordingsInReport() final;

 private:
  const raw_ptr<IdentifiabilityStudyState, DanglingUntriaged>
      identifiability_study_state_;

  // Keeps track of whether Privacy Budget metadata was reported. This flag is
  // reset each time the UKM service constructs a new UKM report. The goal being
  // that each report includes a metadata tag.
  //
  // This flag is meant as an optimization. Ideally every `UkmEntry` should
  // include the metadata, but that leads to a fairly large overhead much of
  // which is redundant.
  bool metadata_reported_ = false;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_UKM_ENTRY_FILTER_H_
