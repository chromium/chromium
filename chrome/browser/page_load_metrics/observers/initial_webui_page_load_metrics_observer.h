// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "content/public/browser/navigation_handle_timing.h"
#include "content/public/browser/site_instance_process_assignment.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class MetricsReporter;
class WaapUIMetricsService;
class InitialWebUIWindowMetricsManager;

namespace content {
class NavigationHandle;
}  // namespace content

namespace ukm::builders {
class InitialWebUIPageLoad;
}  // namespace ukm::builders

// The metrics observer for page loads of InitialWebUI.
//
// A InitialWebUI is a WebUI shown in the top chrome UI, e.g. toolbar. It is
// different from the normal WebUI which is shown in the content area, e.g.
// New Tab Page.
//
// See
// https://docs.google.com/document/d/13nVm0v4hKFfTjbsE0n7loh3seBdRmqyLXByZqjlpc8Q/edit?tab=t.0
class InitialWebUIPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  InitialWebUIPageLoadMetricsObserver();

  // Not movable or copyable.
  InitialWebUIPageLoadMetricsObserver(
      const InitialWebUIPageLoadMetricsObserver&) = delete;
  InitialWebUIPageLoadMetricsObserver& operator=(
      const InitialWebUIPageLoadMetricsObserver&) = delete;

  ~InitialWebUIPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  void OnMonotonicFirstPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnMonotonicFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnUserInput(
      const blink::WebInputEvent& event,
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  ObservePolicy ShouldObserveScheme(const GURL& url) const override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info)
      override;
  void OnCpuTimingUpdate(
      content::RenderFrameHost* subframe_rfh,
      const page_load_metrics::mojom::CpuTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordNavigationTimingMetrics();
  void RecordTimingMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing);
  void RecordPageLoadMetrics(base::TimeTicks app_background_time);
  void RecordRendererUsageMetrics();
  void RecordPageLoadTimestampMetrics(
      ukm::builders::InitialWebUIPageLoad& builder);
  void RecordPageEndMetrics(
      const page_load_metrics::mojom::PageLoadTiming* timing,
      base::TimeTicks page_end_time,
      bool app_entered_background);
  void RecordAbortMetrics(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      base::TimeTicks page_end_time,
      ukm::builders::InitialWebUIPageLoad* builder);

  // Returns the service for the current profile.
  // The service is guaranteed to be non-null.
  WaapUIMetricsService* service() const;

  // Returns the MetricsReporter for the current WebContents.
  // The MetricsReporter is tighted to WebContents, and so is this observer.
  // Thus the MetricsReporter is guaranteed to be non-null.
  MetricsReporter& GetMetricsReporter();

  // Returns the MetricsManager for the current window.
  InitialWebUIWindowMetricsManager* GetMetricsManager() const;

  // Total CPU wall time used by the page while in the foreground.
  base::TimeDelta total_foreground_cpu_time_;

  // Load timing metrics of the main frame resource request.
  content::NavigationHandleTiming navigation_handle_timing_;

  // How the SiteInstance for the committed page was assigned a renderer.
  std::optional<content::SiteInstanceProcessAssignment>
      render_process_assignment_;

  // PAGE_TRANSITION_LINK is the default PageTransition value.
  ui::PageTransition page_transition_ = ui::PAGE_TRANSITION_LINK;

  // True if the page started hidden, or ever became hidden.
  bool was_hidden_ = false;

  // True if the page main resource was served from disk cache.
  bool was_cached_ = false;

  // Set to true if the main frame resource has a 'Cache-control: no-store'
  // response header and set to false otherwise. Not set if there is no response
  // header present.
  std::optional<bool> main_frame_resource_has_no_store_;

  bool currently_in_foreground_ = false;
  // The last time the page became foregrounded, or navigation start if the page
  // started in the foreground and has not been backgrounded.
  base::TimeTicks last_time_shown_;
  base::TimeDelta total_foreground_duration_;

  // The navigation start timestamp.
  base::Time navigation_start_time_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_INITIAL_WEBUI_PAGE_LOAD_METRICS_OBSERVER_H_
