// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_page_load_metrics_observer.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_content_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace {

std::string GetHistogramNamePrefix(previews::PreviewsType previews_type) {
  switch (previews_type) {
    case previews::PreviewsType::NOSCRIPT:
      return "PageLoad.Clients.NoScriptPreview.";
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
      return "PageLoad.Clients.ResourceLoadingHintsPreview.";
    default:
      NOTREACHED();
      return std::string();
  }
}

void RecordPageLoadHistogram(previews::PreviewsType previews_type,
                             const std::string& histogram,
                             base::TimeDelta sample) {
  // Match PAGE_LOAD_HISTOGRAM params:
  UmaHistogramCustomTimes(GetHistogramNamePrefix(previews_type) + histogram,
                          sample, base::TimeDelta::FromMilliseconds(10),
                          base::TimeDelta::FromMinutes(10), 100);
}

void RecordPageSizeHistograms(previews::PreviewsType previews_type,
                              int64_t num_network_resources,
                              int64_t network_bytes) {
  // Match PAGE_BYTES_HISTOGRAM params:
  base::UmaHistogramCustomCounts(
      GetHistogramNamePrefix(previews_type) +
          "Experimental.Bytes.NetworkIncludingHeaders",
      static_cast<int>((network_bytes) / 1024), 1, 500 * 1024, 50);
}

int GetDefaultInflationPercent(previews::PreviewsType previews_type) {
  switch (previews_type) {
    case previews::PreviewsType::NOSCRIPT:
      return previews::params::NoScriptPreviewsInflationPercent();
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
      return previews::params::ResourceLoadingHintsPreviewsInflationPercent();
    default:
      NOTREACHED();
      return 0;
  }
}

int GetDefaultInflationBytes(previews::PreviewsType previews_type) {
  switch (previews_type) {
    case previews::PreviewsType::NOSCRIPT:
      return previews::params::NoScriptPreviewsInflationBytes();
    case previews::PreviewsType::RESOURCE_LOADING_HINTS:
      return previews::params::ResourceLoadingHintsPreviewsInflationBytes();
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace

namespace previews {

PreviewsPageLoadMetricsObserver::PreviewsPageLoadMetricsObserver() {}

PreviewsPageLoadMetricsObserver::~PreviewsPageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground)
    return STOP_OBSERVING;

  if (previews::params::IsNoScriptPreviewsEnabled() ||
      previews::params::IsResourceLoadingHintsEnabled()) {
    return CONTINUE_OBSERVING;
  }

  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return STOP_OBSERVING;

  previews::PreviewsUserData* previews_user_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_user_data)
    return STOP_OBSERVING;

  previews_type_ = previews::GetMainFramePreviewsType(
      previews_user_data->PreHoldbackCommittedPreviewsState());
  if (previews_type_ != previews::PreviewsType::NOSCRIPT &&
      previews_type_ != previews::PreviewsType::RESOURCE_LOADING_HINTS) {
    return STOP_OBSERVING;
  }

  data_savings_inflation_percent_ =
      previews_user_data->data_savings_inflation_percent();

  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PreviewsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification.
  if (GetDelegate().DidCommit()) {
    RecordPageSizeUMA();
    RecordTimingMetrics(timing);
  }
  return STOP_OBSERVING;
}

void PreviewsPageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO(dougarnett): Determine if a different event makes more sense.
  // https://crbug.com/864720
  int64_t inflation_bytes = 0;
  if (data_savings_inflation_percent_ == 0) {
    data_savings_inflation_percent_ =
        GetDefaultInflationPercent(previews_type_);
    inflation_bytes = GetDefaultInflationBytes(previews_type_);
  }

  int64_t total_saved_bytes =
      (total_network_bytes_ * data_savings_inflation_percent_) / 100 +
      inflation_bytes;

  DCHECK(GetDelegate().GetUrl().SchemeIsHTTPOrHTTPS());

  WriteToSavings(GetDelegate().GetUrl(), total_saved_bytes);
}

void PreviewsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordPageSizeUMA();
  RecordTimingMetrics(timing);
}

void PreviewsPageLoadMetricsObserver::RecordPageSizeUMA() const {
  RecordPageSizeHistograms(previews_type_, num_network_resources_,
                           total_network_bytes_);
}

void PreviewsPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    RecordPageLoadHistogram(previews_type_,
                            "DocumentTiming.NavigationToLoadEventFired",
                            timing.document_timing->load_event_start.value());
  }
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    RecordPageLoadHistogram(
        previews_type_, "PaintTiming.NavigationToFirstContentfulPaint",
        timing.paint_timing->first_contentful_paint.value());
  }
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, GetDelegate())) {
    RecordPageLoadHistogram(
        previews_type_,
        "Experimental.PaintTiming.NavigationToFirstMeaningfulPaint",
        timing.paint_timing->first_meaningful_paint.value());
  }
  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, GetDelegate())) {
    RecordPageLoadHistogram(
        previews_type_, "ParseTiming.ParseBlockedOnScriptLoad",
        timing.parse_timing->parse_blocked_on_script_load_duration.value());
    RecordPageLoadHistogram(previews_type_, "ParseTiming.ParseDuration",
                            timing.parse_timing->parse_stop.value() -
                                timing.parse_timing->parse_start.value());
  }
}

void PreviewsPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* rfh,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  for (auto const& resource : resources) {
    if (resource->cache_type ==
            page_load_metrics::mojom::CacheType::kNotCached &&
        resource->is_complete) {
      num_network_resources_++;
    }
    total_network_bytes_ += resource->delta_bytes;
  }
}

void PreviewsPageLoadMetricsObserver::WriteToSavings(const GURL& url,
                                                     int64_t bytes_saved) {
  bool is_https = url.SchemeIs(url::kHttpsScheme);

  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              browser_context_);

  bool data_saver_enabled =
      data_reduction_proxy_settings->IsDataReductionProxyEnabled();

  data_reduction_proxy_settings->data_reduction_proxy_service()
      ->UpdateContentLengths(0, bytes_saved, data_saver_enabled,
                             (is_https ? data_reduction_proxy::HTTPS
                                       : data_reduction_proxy::DIRECT_HTTP),
                             "text/html", true,
                             data_use_measurement::DataUseUserData::OTHER, 0);

  data_reduction_proxy_settings->data_reduction_proxy_service()
      ->UpdateDataUseForHost(0, bytes_saved, url.HostNoBrackets());
}

}  // namespace previews
