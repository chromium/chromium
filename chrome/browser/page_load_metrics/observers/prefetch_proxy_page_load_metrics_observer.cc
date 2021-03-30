// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/prefetch_proxy_page_load_metrics_observer.h"

#include <algorithm>

#include "base/metrics/histogram_macros_local.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_url_loader_interceptor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/history/core/browser/history_service.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/cookies/cookie_options.h"
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

const size_t kUkmCssJsBeforeFcpMax = 10;

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

  CheckForCookiesOnURL(navigation_handle->GetWebContents()->GetBrowserContext(),
                       navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  CheckForCookiesOnURL(navigation_handle->GetWebContents()->GetBrowserContext(),
                       navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrefetchProxyPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!navigation_handle->GetURL().SchemeIsHTTPOrHTTPS())
    return STOP_OBSERVING;

  if (!navigation_handle->IsInMainFrame())
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
  if (!tab_helper)
    return STOP_OBSERVING;
  after_srp_metrics_ = tab_helper->after_srp_metrics();

  data_saver_enabled_at_commit_ = data_reduction_proxy::
      DataReductionProxySettings::IsDataSaverEnabledByUser(
          profile->IsOffTheRecord(), profile->GetPrefs());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile, ServiceAccessType::IMPLICIT_ACCESS);

  // For unit testing.
  if (!history_service)
    return CONTINUE_OBSERVING;

  for (const GURL& url : navigation_handle->GetRedirectChain()) {
    history_service->GetLastVisitToHost(
        url.GetOrigin(), base::Time() /* before_time */,
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
  PrefetchProxyTabHelper* tab_helper = PrefetchProxyTabHelper::FromWebContents(
      navigation_handle->GetWebContents());
  if (!tab_helper)
    return;

  std::unique_ptr<PrefetchProxyTabHelper::AfterSRPMetrics> after_srp_metrics =
      tab_helper->ComputeAfterSRPMetricsBeforeCommit(navigation_handle);
  if (!after_srp_metrics)
    return;

  // Metrics should also be recorded when the navigation failed due to an abort
  // or otherwise. That way, we don't skew the metrics towards only pages that
  // commit.
  after_srp_metrics_ = *after_srp_metrics;

  RecordAfterSRPEvent();
}

void PrefetchProxyPageLoadMetricsObserver::OnEventOccurred(
    page_load_metrics::PageLoadMetricsEvent event) {
  if (event == page_load_metrics::PageLoadMetricsEvent::PREFETCH_LIKELY) {
    GetPrefetchMetrics();
  }
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

void PrefetchProxyPageLoadMetricsObserver::CheckForCookiesOnURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForUrl(browser_context, url);

  partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&PrefetchProxyPageLoadMetricsObserver::OnCookieResult,
                     weak_factory_.GetWeakPtr(), base::Time::Now()));
}

void PrefetchProxyPageLoadMetricsObserver::OnCookieResult(
    base::Time query_start_time,
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  mainframe_had_cookies_ =
      mainframe_had_cookies_.value_or(false) || !cookies.empty();
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

    if (!resource->completed_before_fcp)
      continue;

    if (resource->cache_type ==
        page_load_metrics::mojom::CacheType::kNotCached) {
      loaded_css_js_from_network_before_fcp_++;
    } else {
      loaded_css_js_from_cache_before_fcp_++;
    }
  }
}

void PrefetchProxyPageLoadMetricsObserver::RecordMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_tracker_.TryCancelAll();

  if (mainframe_had_cookies_.has_value()) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies",
        mainframe_had_cookies_.value());
  }

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

  LOCAL_HISTOGRAM_COUNTS_100(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached",
      loaded_css_js_from_cache_before_fcp_);
  LOCAL_HISTOGRAM_COUNTS_100(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached",
      loaded_css_js_from_network_before_fcp_);

  // Only record UKM for Data Saver users.
  if (!data_saver_enabled_at_commit_)
    return;

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
  if (mainframe_had_cookies_.has_value()) {
    int ukm_mainpage_had_cookies = mainframe_had_cookies_.value() ? 1 : 0;
    builder.Setmainpage_request_had_cookies(ukm_mainpage_had_cookies);
  }

  int ukm_loaded_css_js_from_cache_before_fcp =
      std::min(kUkmCssJsBeforeFcpMax, loaded_css_js_from_cache_before_fcp_);
  builder.Setcount_css_js_loaded_cache_before_fcp(
      ukm_loaded_css_js_from_cache_before_fcp);

  int ukm_loaded_css_js_from_network_before_fcp =
      std::min(kUkmCssJsBeforeFcpMax, loaded_css_js_from_network_before_fcp_);
  builder.Setcount_css_js_loaded_network_before_fcp(
      ukm_loaded_css_js_from_network_before_fcp);

  if (srp_metrics_ && srp_metrics_->predicted_urls_count_ > 0) {
    builder.Setordered_eligible_pages_bitmask(
        srp_metrics_->ordered_eligible_pages_bitmask_);
    builder.Setprefetch_eligible_count(srp_metrics_->prefetch_eligible_count_);
    builder.Setprefetch_attempted_count(
        srp_metrics_->prefetch_attempted_count_);
    builder.Setprefetch_successful_count(
        srp_metrics_->prefetch_successful_count_);
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void PrefetchProxyPageLoadMetricsObserver::RecordAfterSRPEvent() {
  if (!after_srp_metrics_)
    return;

  const PrefetchProxyTabHelper::AfterSRPMetrics& metrics = *after_srp_metrics_;

  ukm::builders::PrefetchProxy_AfterSRPClick builder(
      GetDelegate().GetPageUkmSourceId());

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
    builder.SetProbeLatencyMs(metrics.probe_latency_.value().InMilliseconds());
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
