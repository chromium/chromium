// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_loading_page_load_metrics_observer.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "chrome/browser/history/history_service_factory.h"
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

SubresourceLoadingPageLoadMetricsObserver::
    SubresourceLoadingPageLoadMetricsObserver() = default;

SubresourceLoadingPageLoadMetricsObserver::
    ~SubresourceLoadingPageLoadMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  navigation_start_ = base::Time::Now();

  CheckForCookiesOnURL(navigation_handle->GetWebContents()->GetBrowserContext(),
                       currently_committed_url);
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  CheckForCookiesOnURL(navigation_handle->GetWebContents()->GetBrowserContext(),
                       navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::OnCommit(
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
            &SubresourceLoadingPageLoadMetricsObserver::OnOriginLastVisitResult,
            weak_factory_.GetWeakPtr(), base::Time::Now()),
        &task_tracker_);
  }

  return CONTINUE_OBSERVING;
}

void SubresourceLoadingPageLoadMetricsObserver::OnOriginLastVisitResult(
    base::Time query_start_time,
    history::HistoryLastVisitToHostResult result) {
  history_query_times_.push_back(base::Time::Now() - query_start_time);

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

void SubresourceLoadingPageLoadMetricsObserver::CheckForCookiesOnURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForSite(browser_context, url);

  partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, net::CookieOptions::MakeAllInclusive(),
      base::BindOnce(&SubresourceLoadingPageLoadMetricsObserver::OnCookieResult,
                     weak_factory_.GetWeakPtr(), base::Time::Now()));
}

void SubresourceLoadingPageLoadMetricsObserver::OnCookieResult(
    base::Time query_start_time,
    const net::CookieStatusList& cookies,
    const net::CookieStatusList& excluded_cookies) {
  cookie_query_times_.push_back(base::Time::Now() - query_start_time);
  mainframe_had_cookies_ =
      mainframe_had_cookies_.value_or(false) || !cookies.empty();
}

void SubresourceLoadingPageLoadMetricsObserver::OnResourceDataUseObserved(
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

void SubresourceLoadingPageLoadMetricsObserver::RecordMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_tracker_.TryCancelAll();

  for (base::TimeDelta cookie_query_time : cookie_query_times_) {
    UMA_HISTOGRAM_TIMES("PageLoad.Clients.SubresourceLoading.CookiesQueryTime",
                        cookie_query_time);
  }
  cookie_query_times_.clear();
  for (base::TimeDelta history_query_time : history_query_times_) {
    UMA_HISTOGRAM_TIMES("PageLoad.Clients.SubresourceLoading.HistoryQueryTime",
                        history_query_time);
  }
  history_query_times_.clear();

  if (mainframe_had_cookies_.has_value()) {
    UMA_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies",
        mainframe_had_cookies_.value());
  }

  if (min_days_since_last_visit_to_origin_.has_value()) {
    int days_since_last_visit = min_days_since_last_visit_to_origin_.value();

    UMA_HISTOGRAM_BOOLEAN(
        "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin",
        days_since_last_visit != -1);

    if (days_since_last_visit >= 0) {
      UMA_HISTOGRAM_COUNTS_100(
          "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin",
          days_since_last_visit);
    }
  }

  UMA_HISTOGRAM_COUNTS_100(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached",
      loaded_css_js_from_cache_before_fcp_);
  UMA_HISTOGRAM_COUNTS_100(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached",
      loaded_css_js_from_network_before_fcp_);

  // Only record UKM for Data Saver users.
  if (!data_saver_enabled_at_commit_)
    return;

  ukm::builders::PrefetchProxy builder(GetDelegate().GetSourceId());

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

  builder.Record(ukm::UkmRecorder::Get());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
SubresourceLoadingPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
  return STOP_OBSERVING;
}

void SubresourceLoadingPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordMetrics();
}
