// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_PROCESS_SELECTION_DEFERRING_CONDITION_H_
#define CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_PROCESS_SELECTION_DEFERRING_CONDITION_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/site_protection/site_familiarity_fetcher.h"
#include "components/history/core/browser/history_types.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/process_selection_deferring_condition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace site_protection {

// ProcessSelectionDeferringCondition which defers process-selection till the
// site's familiarity is computed.
class SiteFamiliarityProcessSelectionDeferringCondition
    : public content::ProcessSelectionDeferringCondition {
 public:
  explicit SiteFamiliarityProcessSelectionDeferringCondition(
      content::NavigationHandle& navigation_handle);
  ~SiteFamiliarityProcessSelectionDeferringCondition() override;

  // ProcessSelectionDeferringCondition:
  void OnRequestRedirected() override;
  Result OnWillSelectFinalProcess(base::OnceClosure resume) override;

 private:
  void StartFetching();

  // Called when `fetcher_` has finished computing the verdict.
  void OnComputedVerdict(SiteFamiliarityFetcher::Verdict verdict);

  // Sets the verdict on the NavigationHandle.
  void SetVerdictOnHandle();

  SiteFamiliarityFetcher fetcher_;
  std::optional<SiteFamiliarityFetcher::Verdict> verdict_;

  // Callback passed to OnWillSelectFinalProcess().
  base::OnceClosure callback_;

  base::WeakPtrFactory<SiteFamiliarityProcessSelectionDeferringCondition>
      weak_factory_{this};
};

}  // namespace site_protection

#endif  // CHROME_BROWSER_SITE_PROTECTION_SITE_FAMILIARITY_PROCESS_SELECTION_DEFERRING_CONDITION_H_
