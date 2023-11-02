// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/prefetch_proxy_page_load_metrics_observer.h"

#include <algorithm>

#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_url_loader_interceptor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/prefetch_metrics.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace {

// Yields 10 buckets between 1 and 180 (1,2,3,5,9,15,25,42,70,119), per privacy
// requirements. Computed using:
// CEIL(
//   POW(kDaysSinceLastVisitBucketSpacing,
//       FLOOR(LN(sample) / LN(kDaysSinceLastVisitBucketSpacing)))
// )
const double kDaysSinceLastVisitBucketSpacing = 1.7;

bool IsCSSOrJS(const std::string& mime_type) {
  std::string lower_mime_type = base::ToLowerASCII(mime_type);
  return lower_mime_type == "text/css" ||
         blink::IsSupportedJavascriptMimeType(lower_mime_type);
}

}  // namespace

PrefetchProxyPageLoadMetricsObserver::PrefetchProxyPageLoadMetricsObserver() =
    default;

PrefetchProxyPageLoadMetricsObserver::~PrefetchProxyPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_start_ = base::Time::Now();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // All observing events are preprocessed by PageLoadTracker so that the
  // outermost page's observer instance sees gathered information. So, the
  // instance for FencedFrames doesn't need to do anything.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Prefetch proxy is used for privacy preserrving prefetch, and the feature
  // and prerendering are exclusive in speculationrules. So, prerendering
  // doesn't use the proxy, and this class just stops observing here. It's just
  // interested in primary main page's performance with the prefetch proxy.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return STOP_OBSERVING;

  if (!navigation_handle->IsInPrimaryMainFrame())
    return STOP_OBSERVING;

  if (!page_load_metrics::IsNavigationUserInitiated(navigation_handle))
    return STOP_OBSERVING;

  if (!GetDelegate().StartedInForeground())
    return STOP_OBSERVING;

  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());

  if (profile->IsOffTheRecord())
    return STOP_OBSERVING;

  PrefetchProxyTabHelper* tab_helper = PrefetchProxyTabHelper::FromWebContents(
      navigation_handle->GetWebContents());
  if (tab_helper)
    after_srp_metrics_ = tab_helper->after_srp_metrics();

  serving_page_metrics_ =
      content::PrefetchServingPageMetrics::GetForNavigationHandle(
          *navigation_handle);

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile, ServiceAccessType::IMPLICIT_ACCESS);

  // For unit testing.
  if (!history_service)
    return CONTINUE_OBSERVING;

  for (const GURL& url : navigation_handle->GetRedirectChain()) {
    history_service->GetLastVisitToOrigin(
        url::Origin::Create(url), base::Time() /* before_time */,
        navigation_start_ /* end_time */,
        base::BindOnce(
            &PrefetchProxyPageLoadMetricsObserver::OnOriginLastVisitResult,
            weak_factory_.GetWeakPtr(), base::Time::Now()),
        &task_tracker_);
  }

  return CONTINUE_OBSERVING;
}

void PrefetchProxyPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  serving_page_metrics_ =
      content::PrefetchServingPageMetrics::GetForNavigationHandle(
          *navigation_handle);

  PrefetchProxyTabHelper* tab_helper = PrefetchProxyTabHelper::FromWebContents(
      navigation_handle->GetWebContents());
  if (tab_helper) {
    std::unique_ptr<PrefetchProxyTabHelper::AfterSRPMetrics> after_srp_metrics =
        tab_helper->ComputeAfterSRPMetricsBeforeCommit(navigation_handle);
    if (after_srp_metrics) {
      // Metrics should also be recorded when the navigation failed due to an
      // abort or otherwise. That way, we don't skew the metrics towards only
      // pages that commit.
      after_srp_metrics_ = *after_srp_metrics;
    }
  }

  RecordAfterSRPEvent();
}

void PrefetchProxyPageLoadMetricsObserver::OnPrefetchLikely() {
  GetPrefetchMetrics();

  referring_page_metrics_ =
      content::PrefetchReferringPageMetrics::GetForCurrentDocument(
          GetDelegate().GetWebContents()->GetPrimaryMainFrame());
}

void PrefetchProxyPageLoadMetricsObserver::GetPrefetchMetrics() {
  PrefetchProxyTabHelper* tab_helper =
      PrefetchProxyTabHelper::FromWebContents(GetDelegate().GetWebContents());
  if (!tab_helper)
    return;

  srp_metrics_ = tab_helper->page_->srp_metrics_;
}

void PrefetchProxyPageLoadMetricsObserver::OnOriginLastVisitResult(
    base::Time query_start_time,
    history::HistoryLastVisitResult result) {
  if (!result.success)
    return;

  if (!min_days_since_last_visit_to_origin_.has_value()) {
    min_days_since_last_visit_to_origin_ = -1;
  }

  if (result.last_visit.is_null())
    return;

  base::TimeDelta last_visit_delta = base::Time::Now() - result.last_visit;
  int last_visit_in_days = last_visit_delta.InDays();

  if (min_days_since_last_visit_to_origin_.value() == -1 ||
      min_days_since_last_visit_to_origin_.value() > last_visit_in_days) {
    min_days_since_last_visit_to_origin_ = last_visit_in_days;
  }
}

void PrefetchProxyPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (const auto& resource : resources) {
    if (!resource->is_complete)
      continue;

    if (!resource->is_main_frame_resource)
      continue;

    if (!IsCSSOrJS(resource->mime_type))
      continue;
  }
}

void PrefetchProxyPageLoadMetricsObserver::RecordMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_tracker_.TryCancelAll();

  if (min_days_since_last_visit_to_origin_.has_value()) {
    int days_since_last_visit = min_days_since_last_visit_to_origin_.value();

    LOCAL_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin",
        days_since_last_visit != -1);

    if (days_since_last_visit >= 0) {
      LOCAL_HISTOGRAM_COUNTS_100(
          "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin",
          days_since_last_visit);
    }
  }

  RecordPrefetchProxyEvent();
  RecordAfterSRPEvent();
}

void PrefetchProxyPageLoadMetricsObserver::RecordPrefetchProxyEvent() {
  ukm::builders::PrefetchProxy builder(GetDelegate().GetPageUkmSourceId());

  if (min_days_since_last_visit_to_origin_.has_value()) {
    // The -1 value is a sentinel to signal there was no previous visit. Don't
    // let the ukm call make it 0.
    if (min_days_since_last_visit_to_origin_.value() == -1) {
      builder.Setdays_since_last_visit_to_origin(-1);
    } else {
      int64_t maxxed_days_since_last_visit =
          std::min(180, min_days_since_last_visit_to_origin_.value());
      int64_t ukm_days_since_last_visit = ukm::GetExponentialBucketMin(
          maxxed_days_since_last_visit, kDaysSinceLastVisitBucketSpacing);
      builder.Setdays_since_last_visit_to_origin(ukm_days_since_last_visit);
    }
  }

  // We should not have metrics from both the chrome/ and content/
  // implementations of speculation rules prefetch.
  DCHECK(!(srp_metrics_ && srp_metrics_->predicted_urls_count_ > 0 &&
           referring_page_metrics_));

  if (srp_metrics_ && srp_metrics_->predicted_urls_count_ > 0) {
    builder.Setprefetch_eligible_count(srp_metrics_->prefetch_eligible_count_);
    builder.Setprefetch_attempted_count(
        srp_metrics_->prefetch_attempted_count_);
    builder.Setprefetch_successful_count(
        srp_metrics_->prefetch_successful_count_);
  }

  if (referring_page_metrics_) {
    builder.Setprefetch_eligible_count(
        referring_page_metrics_->prefetch_eligible_count);
    builder.Setprefetch_attempted_count(
        referring_page_metrics_->prefetch_attempted_count);
    builder.Setprefetch_successful_count(
        referring_page_metrics_->prefetch_successful_count);
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void PrefetchProxyPageLoadMetricsObserver::RecordAfterSRPEvent() {
  // We should not have metrics from both the chrome/ and content/
  // implementations of speculation rules prefetch.
  DCHECK(!(after_srp_metrics_ && serving_page_metrics_));
  if (!after_srp_metrics_ && !serving_page_metrics_)
    return;

  ukm::builders::PrefetchProxy_AfterSRPClick builder(
      GetDelegate().GetPageUkmSourceId());

  if (after_srp_metrics_) {
    const PrefetchProxyTabHelper::AfterSRPMetrics& metrics =
        *after_srp_metrics_;

    builder.SetSRPPrefetchEligibleCount(metrics.prefetch_eligible_count_);

    if (metrics.prefetch_status_) {
      builder.SetSRPClickPrefetchStatus(
          static_cast<int>(metrics.prefetch_status_.value()));
    }

    if (metrics.clicked_link_srp_position_) {
      builder.SetClickedLinkSRPPosition(
          metrics.clicked_link_srp_position_.value());
    }

    if (metrics.probe_latency_) {
      builder.SetProbeLatencyMs(
          metrics.probe_latency_.value().InMilliseconds());
    }
  }

  if (serving_page_metrics_) {
    if (serving_page_metrics_->prefetch_status) {
      builder.SetSRPClickPrefetchStatus(
          serving_page_metrics_->prefetch_status.value());
    }

    if (serving_page_metrics_->required_private_prefetch_proxy) {
      builder.SetPrivatePrefetch(1);
    }

    if (serving_page_metrics_->same_tab_as_prefetching_tab) {
      builder.SetSameTabAsPrefetchingTab(1);
    }

    if (serving_page_metrics_->prefetch_header_latency) {
      builder.SetPrefetchHeaderLatencyMs(
          serving_page_metrics_->prefetch_header_latency->InMilliseconds());
    }

    if (serving_page_metrics_->probe_latency) {
      builder.SetProbeLatencyMs(
          serving_page_metrics_->probe_latency.value().InMilliseconds());
    }
  }

  builder.Record(ukm::UkmRecorder::Get());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

void PrefetchProxyPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
}
