// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_protection/site_familiarity_process_selection_deferring_condition.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_protection/site_familiarity_fetcher.h"
#include "chrome/browser/site_protection/site_familiarity_process_selection_user_data.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/schemeful_site.h"
#include "url/origin.h"

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

  if (IsDefaultSearchEngineUrl(
          navigation_handle().GetURL(),
          Profile::FromBrowserContext(
              navigation_handle().GetWebContents()->GetBrowserContext()))) {
    // This histogram will always record "did not defer" whenever the feature
    // kSkipSiteFamiliarityDeferralForDefaultSearchEngine is enabled.
    // TODO(crbug.com/507810928): Clean up this histogram after reaching stable.
    UMA_HISTOGRAM_BOOLEAN(
        kSiteFamiliarityDeferNavigationForDefaultSearchEngineHistogram,
        !verdict_);
  }

  if (verdict_) {
    SetVerdictOnHandle();
    return content::ProcessSelectionDeferringCondition::Result::kProceed;
  }
  callback_ = std::move(resume);
  defer_timer_.emplace();
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
    if (defer_timer_) {
      base::UmaHistogramTimes(kSiteFamiliarityDeferNavigationDurationHistogram,
                              defer_timer_->Elapsed());
      defer_timer_.reset();
    }
    std::move(callback_).Run();
  }
}

void SiteFamiliarityProcessSelectionDeferringCondition::SetVerdictOnHandle() {
  SiteFamiliarityFetcher::Verdict verdict = verdict_.value();
  bool is_familiar = verdict == SiteFamiliarityFetcher::Verdict::kFamiliar;

  navigation_handle().GetProcessSelectionUserData().SetUserData(
      &SiteFamiliarityProcessSelectionUserData::kUserDataKey,
      std::make_unique<SiteFamiliarityProcessSelectionUserData>(
          /*is_site_familiar=*/is_familiar));

  Profile* profile = Profile::FromBrowserContext(
      navigation_handle().GetWebContents()->GetBrowserContext());
  if (profile && !profile->IsOffTheRecord()) {
    bool is_top_frame = navigation_handle().IsInPrimaryMainFrame();
    bool is_cross_site_subframe = IsCrossSiteSubframe();

    if (is_top_frame) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.SiteFamiliarity.Verdict.TopFrame", verdict);
    } else if (is_cross_site_subframe) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.SiteFamiliarity.Verdict.Subframe", verdict);
    }

    if (is_top_frame || is_cross_site_subframe) {
      base::UmaHistogramEnumeration("SafeBrowsing.SiteFamiliarity.Verdict",
                                    verdict);
    }
  }
}

bool SiteFamiliarityProcessSelectionDeferringCondition::IsCrossSiteSubframe()
    const {
  if (navigation_handle().IsInPrimaryMainFrame()) {
    return false;
  }
  content::RenderFrameHost* parent_frame = navigation_handle().GetParentFrame();
  if (!parent_frame) {
    return false;
  }
  content::RenderFrameHost* topmost_frame =
      parent_frame->GetOutermostMainFrame();
  if (!topmost_frame->IsInPrimaryMainFrame()) {
    return false;
  }
  std::optional<url::Origin> subframe_origin =
      navigation_handle().GetOriginToCommit();
  if (!subframe_origin) {
    return false;
  }
  url::Origin topmost_origin = topmost_frame->GetLastCommittedOrigin();
  return !net::SchemefulSite::IsSameSite(subframe_origin.value(),
                                         topmost_origin);
}

}  //  namespace site_protection
