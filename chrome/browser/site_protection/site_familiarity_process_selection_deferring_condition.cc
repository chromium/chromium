// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_process_selection_deferring_condition.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_protection/site_familiarity_fetcher.h"
#include "chrome/browser/site_protection/site_familiarity_process_selection_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace site_protection {

SiteFamiliarityProcessSelectionDeferringCondition::
    SiteFamiliarityProcessSelectionDeferringCondition(
        content::NavigationHandle& navigation_handle)
    : content::ProcessSelectionDeferringCondition(navigation_handle),
      fetcher_(Profile::FromBrowserContext(
          navigation_handle.GetWebContents()->GetBrowserContext())) {
  StartFetching();
}

SiteFamiliarityProcessSelectionDeferringCondition::
    ~SiteFamiliarityProcessSelectionDeferringCondition() = default;

void SiteFamiliarityProcessSelectionDeferringCondition::OnRequestRedirected() {
  if (fetcher_.fetch_url() == navigation_handle().GetURL()) {
    return;
  }
  StartFetching();
}

SiteFamiliarityProcessSelectionDeferringCondition::Result
SiteFamiliarityProcessSelectionDeferringCondition::OnWillSelectFinalProcess(
    base::OnceClosure resume) {
  UMA_HISTOGRAM_BOOLEAN(
      "SafeBrowsing.V8Optimizer.DeferNavigationToComputeSiteFamiliarity",
      !verdict_);
  if (verdict_) {
    SetVerdictOnHandle();
    return content::ProcessSelectionDeferringCondition::Result::kProceed;
  }
  callback_ = std::move(resume);
  return content::ProcessSelectionDeferringCondition::Result::kDefer;
}

void SiteFamiliarityProcessSelectionDeferringCondition::StartFetching() {
  verdict_ = std::nullopt;
  fetcher_.Start(
      navigation_handle().GetURL(),
      base::BindOnce(
          &SiteFamiliarityProcessSelectionDeferringCondition::OnComputedVerdict,
          weak_factory_.GetWeakPtr()));
}

void SiteFamiliarityProcessSelectionDeferringCondition::OnComputedVerdict(
    SiteFamiliarityFetcher::Verdict verdict) {
  verdict_ = verdict;

  if (callback_) {
    SetVerdictOnHandle();
    std::move(callback_).Run();
  }
}

void SiteFamiliarityProcessSelectionDeferringCondition::SetVerdictOnHandle() {
  navigation_handle().GetProcessSelectionUserData().SetUserData(
      &SiteFamiliarityProcessSelectionUserData::kUserDataKey,
      std::make_unique<SiteFamiliarityProcessSelectionUserData>(
          /*is_site_familiar=*/(verdict_.value() ==
                                SiteFamiliarityFetcher::Verdict::kFamiliar)));
}

}  //  namespace site_protection
