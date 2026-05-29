// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/ads_handler.h"

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/observers/ad_metrics/ads_page_load_metrics_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

AdsHandler::AdsHandler(content::WebContents* web_contents,
                       protocol::UberDispatcher* dispatcher,
                       bool is_trusted)
    : content::WebContentsObserver(web_contents) {
  protocol::Ads::Dispatcher::wire(dispatcher, this);
}

AdsHandler::~AdsHandler() = default;

protocol::Response AdsHandler::GetAdMetrics(
    std::unique_ptr<protocol::Ads::AdMetrics>* out_metrics) {
  auto* ads_observer = GetAdsPageLoadMetricsObserver();
  if (!ads_observer) {
    // For simplicity and consistent error handling, return zeroed metrics and a
    // success status.
    *out_metrics = protocol::Ads::AdMetrics::Create()
                       .SetViewportAdDensityByArea(0)
                       .SetAverageViewportAdDensityByArea(0)
                       .SetViewportAdCount(0)
                       .SetAverageViewportAdCount(0)
                       .SetTotalAdCpuTime(0)
                       .SetTotalAdNetworkBytes(0)
                       .Build();
    return protocol::Response::Success();
  }

  auto density_stats = ads_observer->GetLiveStats();

  *out_metrics =
      protocol::Ads::AdMetrics::Create()
          .SetViewportAdDensityByArea(density_stats.viewport_ad_density_by_area)
          .SetAverageViewportAdDensityByArea(
              density_stats.average_viewport_ad_density_by_area)
          .SetViewportAdCount(density_stats.viewport_ad_count)
          .SetAverageViewportAdCount(density_stats.average_viewport_ad_count)
          .SetTotalAdCpuTime(
              ads_observer->GetTotalAdCpuTime().InMillisecondsF())
          .SetTotalAdNetworkBytes(ads_observer->GetTotalAdNetworkBytes())
          .Build();

  return protocol::Response::Success();
}

page_load_metrics::AdsPageLoadMetricsObserver*
AdsHandler::GetAdsPageLoadMetricsObserver() {
  if (!web_contents()) {
    return nullptr;
  }

  content::RenderFrameHost* main_frame = web_contents()->GetPrimaryMainFrame();
  if (!main_frame) {
    return nullptr;
  }

  auto* metrics_web_contents_observer =
      page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents());
  if (!metrics_web_contents_observer) {
    return nullptr;
  }

  base::WeakPtr<page_load_metrics::PageLoadMetricsObserverInterface>
      ads_observer = metrics_web_contents_observer->GetMetricsObserver(
          main_frame,
          page_load_metrics::AdsPageLoadMetricsObserver::kObserverName);
  if (ads_observer) {
    return static_cast<page_load_metrics::AdsPageLoadMetricsObserver*>(
        ads_observer.get());
  }
  return nullptr;
}
