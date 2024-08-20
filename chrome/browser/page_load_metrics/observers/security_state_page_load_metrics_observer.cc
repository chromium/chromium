// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/security_state_page_load_metrics_observer.h"

#include <cmath>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {
// Site Engagement score behavior histogram prefixes.
const char kEngagementFinalPrefix[] = "Security.SiteEngagement";

// Navigation histogram prefixes.
const char kPageEndReasonPrefix[] = "Security.PageEndReason";
const char kTimeOnPagePrefix[] = "Security.TimeOnPage2";

// Security level histograms.
const char kSecurityLevelOnCommit[] = "Security.SecurityLevel.OnCommit";
const char kSecurityLevelOnComplete[] = "Security.SecurityLevel.OnComplete";

}  // namespace

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
SecurityStatePageLoadMetricsObserver::MaybeCreateForProfile(
    content::BrowserContext* profile) {
  // If the site engagement service is not enabled, this observer will not track
  // site engagement metrics, but will still track the security level and
  // navigation related metrics.
  if (!site_engagement::SiteEngagementService::IsEnabled())
    return std::make_unique<SecurityStatePageLoadMetricsObserver>(nullptr);
  auto* engagement_service =
      site_engagement::SiteEngagementServiceFactory::GetForProfile(
          static_cast<Profile*>(profile));
  return std::make_unique<SecurityStatePageLoadMetricsObserver>(
      engagement_service);
}

// static
std::string
SecurityStatePageLoadMetricsObserver::GetEngagementFinalHistogramNameForTesting(
    security_state::SecurityLevel level) {
  return security_state::GetSecurityLevelHistogramName(
      kEngagementFinalPrefix, level);
}

// static
std::string SecurityStatePageLoadMetricsObserver::
    GetSecurityLevelPageEndReasonHistogramNameForTesting(
        security_state::SecurityLevel level) {
  return security_state::GetSecurityLevelHistogramName(
      kPageEndReasonPrefix, level);
}

// static
std::string SecurityStatePageLoadMetricsObserver::
    GetSafetyTipPageEndReasonHistogramNameForTesting(
        security_state::SafetyTipStatus safety_tip_status) {
  return security_state::GetSafetyTipHistogramName(kPageEndReasonPrefix,
                                                   safety_tip_status);
}

SecurityStatePageLoadMetricsObserver::SecurityStatePageLoadMetricsObserver(
    site_engagement::SiteEngagementService* engagement_service)
    : content::WebContentsObserver(), engagement_service_(engagement_service) {}

SecurityStatePageLoadMetricsObserver::~SecurityStatePageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SecurityStatePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (engagement_service_) {
    initial_engagement_score_ =
        engagement_service_->GetScore(navigation_handle->GetURL());
  }
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SecurityStatePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // All data aggregation are done in SiteEngagementService and
  // SecurityStateTabHelper, and this class just monitors the timings to record
  // the aggregated data. As the outermost page's OnCommit and OnComplete are
  // the timing this class is interested in, it just stops observing
  // FencedFrames.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SecurityStatePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Continue observing but keep silence until the page is activated to be the
  // primary page.
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SecurityStatePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  // Only navigations committed to outermost frames are monitored.
  DCHECK(navigation_handle->IsInPrimaryMainFrame() ||
         navigation_handle->IsInPrerenderedMainFrame());

  if (navigation_handle->IsInPrerenderedMainFrame()) {
    // Postpone initialization to activation.
    return CONTINUE_OBSERVING;
  }

  RecordSecurityLevelHistogram(navigation_handle);
  return CONTINUE_OBSERVING;
}

void SecurityStatePageLoadMetricsObserver::DidActivatePrerenderedPage(
    content::NavigationHandle* navigation_handle) {
  // Records current security level as the initial commit time security level.
  // As we don't permit any navigation and cross-origin sub-frames while
  // prerendering, this would not divert from the original security level.
  RecordSecurityLevelHistogram(navigation_handle);
}

void SecurityStatePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!GetDelegate().DidCommit())
    return;

  // Don't record UKMs while prerendering. Also, we dispose data if the
  // prerendered page is not used.
  if (GetDelegate().GetPrerenderingState() ==
      page_load_metrics::PrerenderingState::kInPrerendering) {
    return;
  }

  security_state::SafetyTipStatus safety_tip_status =
      security_state_tab_helper_->GetVisibleSecurityState()
          ->safety_tip_info.status;

  if (engagement_service_) {
    double final_engagement_score =
        engagement_service_->GetScore(GetDelegate().GetUrl());
    // Round the engagement score down to the closest multiple of 10 to decrease
    // the granularity of the UKM collection.
    int64_t coarse_engagement_score =
        ukm::GetLinearBucketMin(final_engagement_score, 10);

    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    ukm::builders::Security_SiteEngagement(GetDelegate().GetPageUkmSourceId())
        .SetInitialSecurityLevel(initial_security_level_)
        .SetFinalSecurityLevel(current_security_level_)
        .SetSafetyTipStatus(static_cast<int64_t>(safety_tip_status))
        .SetScoreDelta(final_engagement_score - initial_engagement_score_)
        .SetScoreFinal(coarse_engagement_score)
        .Record(ukm_recorder);

    // Get the change in Site Engagement score and transform it into the range
    // [0, 100] so it can be logged in an EXACT_LINEAR histogram.
    base::UmaHistogramExactLinear(
        security_state::GetSecurityLevelHistogramName(kEngagementFinalPrefix,
                                                      current_security_level_),
        final_engagement_score, 100);
    base::UmaHistogramExactLinear(
        security_state::GetSafetyTipHistogramName(kEngagementFinalPrefix,
                                                  safety_tip_status),
        final_engagement_score, 100);
  }

  // Record security level UMA histograms.
  base::UmaHistogramEnumeration(
      security_state::GetSecurityLevelHistogramName(kPageEndReasonPrefix,
                                                    current_security_level_),
      GetDelegate().GetPageEndReason(),
      page_load_metrics::PAGE_END_REASON_COUNT);
  base::UmaHistogramCustomTimes(
      security_state::GetSecurityLevelHistogramName(kTimeOnPagePrefix,
                                                    current_security_level_),
      GetDelegate().GetVisibilityTracker().GetForegroundDuration(),
      base::Milliseconds(1), base::Hours(1), 100);
  base::UmaHistogramEnumeration(kSecurityLevelOnComplete,
                                current_security_level_,
                                security_state::SECURITY_LEVEL_COUNT);

  // Record Safety Tip UMA histograms.
  base::UmaHistogramEnumeration(security_state::GetSafetyTipHistogramName(
                                    kPageEndReasonPrefix, safety_tip_status),
                                GetDelegate().GetPageEndReason(),
                                page_load_metrics::PAGE_END_REASON_COUNT);
  base::UmaHistogramCustomTimes(
      security_state::GetSafetyTipHistogramName(kTimeOnPagePrefix,
                                                safety_tip_status),
      GetDelegate().GetVisibilityTracker().GetForegroundDuration(),
      base::Milliseconds(1), base::Hours(1), 100);
}

void SecurityStatePageLoadMetricsObserver::DidChangeVisibleSecurityState() {
  DCHECK_NE(GetDelegate().GetPrerenderingState(),
            page_load_metrics::PrerenderingState::kInPrerendering);
  if (!security_state_tab_helper_)
    return;

  current_security_level_ = security_state_tab_helper_->GetSecurityLevel();
}

void SecurityStatePageLoadMetricsObserver::RecordSecurityLevelHistogram(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  DCHECK(web_contents);
  Observe(web_contents);

  // Gather initial security level after all server redirects have been
  // resolved.
  security_state_tab_helper_ =
      SecurityStateTabHelper::FromWebContents(web_contents);
  // TODO(https://crbug.com/355894536): There are some features that currently
  // instantiate a SecurityStatePageLoadMetricsObserver without a
  // ChromeSecurityStateTabHelper. This does not make sense conceptually. For
  // now add an early return.
  if (!security_state_tab_helper_) {
    return;
  }

  DCHECK_EQ(initial_security_level_, security_state::NONE);
  DCHECK_EQ(current_security_level_, security_state::NONE);
  initial_security_level_ = security_state_tab_helper_->GetSecurityLevel();
  current_security_level_ = initial_security_level_;

  base::UmaHistogramEnumeration(kSecurityLevelOnCommit, initial_security_level_,
                                security_state::SECURITY_LEVEL_COUNT);
}
