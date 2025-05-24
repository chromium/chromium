// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_SERVICE_LEVEL_PAGE_LOAD_METRICS_OBSERVER_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_SERVICE_LEVEL_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "content/public/browser/navigation_discard_reason.h"

namespace android_webview {

// Observes and records metrics for page load attempts in WebView, in order to
// measure and provide observability into the experience of users through
// Customer Centric SLOs.
class ServiceLevelPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(WebViewPageAttemptResult)
  enum class WebViewPageAttemptResult {
    kSuccess = 0,
    kEarlyFinish = 1,
    kFailure = 2,
    kMaxValue = kFailure,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:WebViewPageAttemptResult)

  ServiceLevelPageLoadMetricsObserver() = default;
  ServiceLevelPageLoadMetricsObserver(
      const ServiceLevelPageLoadMetricsObserver&) = delete;
  ~ServiceLevelPageLoadMetricsObserver() override = default;

  ServiceLevelPageLoadMetricsObserver& operator=(
      const ServiceLevelPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override;

  // Prerender, fenced-frame cases are excluded.
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

 private:
  void LogPageAttemptEnd(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      const page_load_metrics::PageLoadMetricsObserverDelegate& delegate);
  void LogPageAttemptEnd(content::NavigationDiscardReason discard_reason);
  void LogPageAttemptEnd(WebViewPageAttemptResult result);

  bool logged_end_ = false;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_SERVICE_LEVEL_PAGE_LOAD_METRICS_OBSERVER_H_
