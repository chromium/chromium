// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_UKM_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/metrics/ukm_source_id.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "net/http/http_response_info.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/page_transition_types.h"

namespace content {
class BrowserContext;
}

namespace network {
class NetworkQualityTracker;
}

namespace ukm {
namespace builders {
class PageLoad;
}
}  // namespace ukm

// If URL-Keyed-Metrics (UKM) is enabled in the system, this is used to
// populate it with top-level page-load metrics.
class UkmPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a UkmPageLoadMetricsObserver, or nullptr if it is not needed.
  static std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
  CreateIfNeeded();

  explicit UkmPageLoadMetricsObserver(
      network::NetworkQualityTracker* network_quality_tracker);
  ~UkmPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;

  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;

  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info)
      override;

  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnResourceDataUseObserved(
      content::RenderFrameHost* content,
      const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
          resources) override;

  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;

  void OnTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnDidFinishSubFrameNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override;

  // Whether the current page load is an Offline Preview. Must be called from
  // OnCommit. Virtual for testing.
  virtual bool IsOfflinePreview(content::WebContents* web_contents) const;

 private:
  // Records page load timing related metrics available in PageLoadTiming, such
  // as first contentful paint.
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Records metrics based on the page load information exposed by the observer
  // delegate, as well as updating the URL. |app_background_time| should be set
  // to a timestamp if the app was backgrounded, otherwise it should be set to
  // a null TimeTicks.
  void RecordPageLoadMetrics(base::TimeTicks app_background_time);

  // Adds main resource timing metrics to |builder|.
  void ReportMainResourceTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      ukm::builders::PageLoad* builder);

  void ReportLayoutStability();

  // Captures the site engagement score for the commited URL and
  // returns the score rounded to the nearest 10.
  base::Optional<int64_t> GetRoundedSiteEngagementScore() const;

  // Returns whether third party cookie blocking is enabled for the committed
  // URL. This is only recorded for users who have prefs::kCookieControlsEnabled
  // set to true.
  base::Optional<bool> GetThirdPartyCookieBlockingEnabled() const;

  // Records the metrics for the nostate prefetch to an event with UKM source ID
  // |source_id|.
  void RecordNoStatePrefetchMetrics(
      content::NavigationHandle* navigation_handle,
      ukm::SourceId source_id);

  // Guaranteed to be non-null during the lifetime of |this|.
  network::NetworkQualityTracker* network_quality_tracker_;

  // The number of body (not header) prefilter bytes consumed by requests for
  // the page.
  int64_t cache_bytes_ = 0;
  int64_t network_bytes_ = 0;

  // Network quality estimates.
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  base::Optional<int32_t> http_response_code_;
  base::Optional<base::TimeDelta> http_rtt_estimate_;
  base::Optional<base::TimeDelta> transport_rtt_estimate_;
  base::Optional<int32_t> downstream_kbps_estimate_;

  // Total CPU wall time used by the page while in the foreground.
  base::TimeDelta total_foreground_cpu_time_;

  // Load timing metrics of the main frame resource request.
  base::Optional<net::LoadTimingInfo> main_frame_timing_;

  // PAGE_TRANSITION_LINK is the default PageTransition value.
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;

  // True if the page started hidden, or ever became hidden.
  bool was_hidden_ = false;

  // True if the page main resource was served from disk cache.
  bool was_cached_ = false;

  // True if the page main resource is inner response of a signed exchange.
  bool is_signed_exchange_inner_response_ = false;

  // The number of main frame redirects that occurred before commit.
  uint32_t main_frame_request_redirect_count_ = 0;

  // The browser context this navigation is operating in.
  content::BrowserContext* browser_context_ = nullptr;

  // Whether the navigation resulted in the main frame being hosted in
  // a different process.
  bool navigation_is_cross_process_ = false;

  // Difference between indices of the previous and current navigation entries
  // (i.e. item history for the current tab).
  // Typically -1/0/1 for back navigations / reloads / forward navigations.
  // 0 for most of navigations with replacement (e.g. location.replace).
  // 1 for regular navigations (link click / omnibox / etc).
  int navigation_entry_offset_ = 0;

  // Id for the main document, which persists across history navigations to the
  // same document.
  // Unique across the lifetime of the browser process.
  int main_document_sequence_number_ = -1;

  // The connection info for the committed URL.
  base::Optional<net::HttpResponseInfo::ConnectionInfo> connection_info_;

  page_load_metrics::LargestContentfulPaintHandler
      largest_contentful_paint_handler_;

  DISALLOW_COPY_AND_ASSIGN(UkmPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
