// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_UKM_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "components/page_load_metrics/browser/navigation_handle_user_data.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/site_instance_process_assignment.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_connection_info.h"
#include "net/nqe/effective_connection_type.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/perfetto/include/perfetto/tracing/event_context.h"
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

  UkmPageLoadMetricsObserver(const UkmPageLoadMetricsObserver&) = delete;
  UkmPageLoadMetricsObserver& operator=(const UkmPageLoadMetricsObserver&) =
      delete;

  ~UkmPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;

  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;

  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;

  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy OnShown() override;

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

  void SetUpSharedMemoryForSmoothness(
      const base::ReadOnlySharedMemoryRegion& shared_memory) override;

  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override;

  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  void OnSoftNavigationUpdated(
      const page_load_metrics::mojom::SoftNavigationMetrics&) override;

  // Whether the current page load is an Offline Preview. Must be called from
  // OnCommit. Virtual for testing.
  virtual bool IsOfflinePreview(content::WebContents* web_contents) const;

 private:
  void RecordNavigationTimingMetrics();

  // Records page load timing related metrics available in PageLoadTiming, such
  // as first contentful paint.
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  // Records page load internal timing metrics, which are used for debugging.
  void RecordInternalTimingMetrics(
      const page_load_metrics::ContentfulPaintTimingInfo&
          all_frames_largest_contentful_paint);

  // Records metrics based on the page load information exposed by the observer
  // delegate, as well as updating the URL. |app_background_time| should be set
  // to a timestamp if the app was backgrounded, otherwise it should be set to
  // a null TimeTicks.
  void RecordPageLoadMetrics(base::TimeTicks app_background_time);

  // Records metrics related to how the renderer process was used for the
  // navigation.
  void RecordRendererUsageMetrics();

  // Adds main resource timing metrics to |builder|.
  void ReportMainResourceTimingMetrics(ukm::builders::PageLoad& builder);

  void ReportLayoutStability();

  // Returns the current Core Web Vital definition of Cumulative Layout Shift.
  // Returns nullopt if current value should not be reported to UKM.
  std::optional<float> GetCoreWebVitalsCLS();
  std::optional<float> GetCoreWebVitalsSoftNavigationIntervalCLS();

  // Returns the current Core Web Vital definition of Largest Contentful Paint.
  // The caller needs to check whether the value should be reported to UKM based
  // on when the page was backgrounded and other validations.
  const page_load_metrics::ContentfulPaintTimingInfo&
  GetCoreWebVitalsLcpTimingInfo();

  const page_load_metrics::ContentfulPaintTimingInfo&
  GetSoftNavigationLargestContentfulPaint() const;

  void RecordSoftNavigationMetrics(
      ukm::SourceId ukm_source_id,
      page_load_metrics::mojom::SoftNavigationMetrics& soft_navigation_metrics);

  void RecordResponsivenessMetricsBeforeSoftNavigationForMainFrame();

  void RecordLayoutShiftBeforeSoftNavigationForMainFrame();

  void RecordAbortMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeTicks page_end_time,
      ukm::builders::PageLoad* builder);

  void RecordMemoriesMetrics(
      ukm::builders::PageLoad& builder,
      const page_load_metrics::PageEndReason page_end_reason);

  void RecordSmoothnessMetrics();
  void RecordResponsivenessMetrics();

  void RecordPageLoadTimestampMetrics(ukm::builders::PageLoad& builder);

  // Captures the site engagement score for the committed URL and
  // returns the score rounded to the nearest 10.
  std::optional<int64_t> GetRoundedSiteEngagementScore() const;

  // Returns whether third party cookie blocking is enabled for the committed
  // URL. This is only recorded for users who have prefs::kCookieControlsEnabled
  // set to true.
  std::optional<bool> GetThirdPartyCookieBlockingEnabled() const;

  // Records the metrics for the nostate prefetch to an event with UKM source ID
  // |source_id|.
  void RecordNoStatePrefetchMetrics(
      content::NavigationHandle* navigation_handle,
      ukm::SourceId source_id);

  // Records the metrics related to Generate URLs (Home page, default search
  // engine) for starting URL and committed URL.
  void RecordGeneratedNavigationUKM(ukm::SourceId source_id,
                                    const GURL& committed_url);

  // Records some metrics at the end of a page, even for failed provisional
  // loads.
  void RecordPageEndMetrics(
      const page_load_metrics::mojom::PageLoadTiming* timing,
      base::TimeTicks page_end_time,
      bool app_entered_background);

  // Records a score from the SiteEngagementService. Called when the page
  // becomes hidden, or at the end of the session if the page is never hidden.
  void RecordSiteEngagement() const;

  // Starts an async call to the cookie manager to determine if there are likely
  // to be cookies set on a mainframe request |url|. This is called on
  // navigation start and redirects but should not be called on commit because
  // it'll get cookies from the mainframe response, if any.
  void UpdateMainFrameRequestHadCookie(content::BrowserContext* browser_context,
                                       const GURL& url);

  // Used as a callback for the cookie manager query.
  void OnMainFrameRequestHadCookieResult(
      base::Time query_start_time,
      const net::CookieAccessResultList& cookies,
      const net::CookieAccessResultList& excluded_cookies);

  // Record some experimental cumulative shift metrics that have occurred on
  // the page until the first time the page moves from the foreground to the
  // background.
  void ReportLayoutInstabilityAfterFirstForeground();

  // Record some largest contentful paint metrics that have occurred on the
  // page until the first time the page starts in the foreground and moves to
  // the background.
  void ReportLargestContentfulPaintAfterFirstForeground();

  // Record some Responsiveness metrics that have occurred on the page until
  // the first time the page moves from the foreground to the background.
  void ReportResponsivenessAfterFirstForeground();

  // Tracing helper for key page load events.
  void EmitUserTimingEvent(base::TimeDelta duration, const char event_name[]);

  // Guaranteed to be non-null during the lifetime of |this|.
  raw_ptr<network::NetworkQualityTracker> network_quality_tracker_;

  // The ID of this navigation, as recorded at each navigation start.
  int64_t navigation_id_ = -1;

  // The number of body (not header) prefilter bytes consumed by requests for
  // the page.
  int64_t cache_bytes_ = 0;
  int64_t network_bytes_ = 0;

  // Sum of decoded body lengths of JS resources in bytes.
  int64_t js_decoded_bytes_ = 0;

  // Max decoded body length of JS resources in bytes.
  int64_t js_max_decoded_bytes_ = 0;

  // Network data use broken down by resource type.
  int64_t image_total_bytes_ = 0;
  int64_t image_subframe_bytes_ = 0;
  int64_t media_bytes_ = 0;

  // Network quality estimates.
  net::EffectiveConnectionType effective_connection_type_ =
      net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
  std::optional<int32_t> http_response_code_;
  std::optional<base::TimeDelta> http_rtt_estimate_;
  std::optional<base::TimeDelta> transport_rtt_estimate_;
  std::optional<int32_t> downstream_kbps_estimate_;

  // Total CPU wall time used by the page while in the foreground.
  base::TimeDelta total_foreground_cpu_time_;

  // Load timing metrics of the main frame resource request.
  content::NavigationHandleTiming navigation_handle_timing_;
  std::optional<net::LoadTimingInfo> main_frame_timing_;

  // First contentful paint as reported in OnFirstContentfulPaintInPage.
  std::optional<base::TimeDelta> first_contentful_paint_;

  // How the SiteInstance for the committed page was assigned a renderer.
  std::optional<content::SiteInstanceProcessAssignment>
      render_process_assignment_;

  // PAGE_TRANSITION_LINK is the default PageTransition value.
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;

  // True if the page started hidden, or ever became hidden.
  bool was_hidden_ = false;

  // True if the page main resource was served from disk cache.
  bool was_cached_ = false;

  // True if the navigation is a reload after the page has been discarded.
  bool was_discarded_ = false;

  // Whether the first URL in the redirect chain matches the default search
  // engine template.
  bool start_url_is_default_search_ = false;

  // Whether the first URL in the redirect chain matches the user's home page
  // URL.
  bool start_url_is_home_page_ = false;

  // The number of main frame redirects that occurred before commit.
  uint32_t main_frame_request_redirect_count_ = 0;

  // Set to true if any main frame request in the redirect chain had cookies set
  // on the request. Set to false if there were no cookies set. Not set if we
  // didn't get a response from the CookieManager before recording metrics.
  std::optional<bool> main_frame_request_had_cookies_;

  // Set to true if the main frame resource has a 'Cache-control: no-store'
  // response header and set to false otherwise. Not set if there is no response
  // header present.
  std::optional<bool> main_frame_resource_has_no_store_;

  // The browser context this navigation is operating in.
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

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

  bool currently_in_foreground_ = false;
  // The last time the page became foregrounded, or navigation start if the page
  // started in the foreground and has not been backgrounded.
  base::TimeTicks last_time_shown_;
  base::TimeDelta total_foreground_duration_;

  // The navigation start timestamp.
  base::Time navigation_start_time_;

  // The connection info for the committed URL.
  std::optional<net::HttpConnectionInfo> connection_info_;

  base::ReadOnlySharedMemoryMapping ukm_smoothness_data_;

  // Only true if the page became hidden after the first time it was shown in
  // the foreground, no matter how it started.
  bool was_hidden_after_first_show_in_foreground = false;

  // True if the TemplateURLService has a search engine template for the
  // navigation and a scoped search would have been possible.
  bool was_scoped_search_like_navigation_ = false;

  // True if the refresh rate is capped at 30Hz because of power saver mode when
  // navigation starts. It is possible but very unlikely for this to change mid
  // navigation, for instance due to a change by the user in settings or the
  // battery being recharged above 20%.
  bool refresh_rate_throttled_ = false;

  // The type of initiator starts the navigation, for more details, please refer
  // to `page_load_metrics::NavigationHandleUserData::InitiatorLocation`.
  page_load_metrics::NavigationHandleUserData::InitiatorLocation
      navigation_trigger_type_ = page_load_metrics::NavigationHandleUserData::
          InitiatorLocation::kOther;

  base::WeakPtrFactory<UkmPageLoadMetricsObserver> weak_factory_{this};
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CORE_UKM_PAGE_LOAD_METRICS_OBSERVER_H_
