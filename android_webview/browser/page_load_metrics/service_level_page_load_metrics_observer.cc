// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/service_level_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace android_webview {

namespace {
constexpr char kPageAttemptsHistogram[] =
    "PageLoad.Clients.WebView.PageAttempts";
constexpr char kPageAttemptResultsHistogram[] =
    "PageLoad.Clients.WebView.PageAttemptResults";
}  // namespace

const char* ServiceLevelPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "ServiceLevelPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceLevelPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (!started_in_foreground) {
    return STOP_OBSERVING;
  }
  base::UmaHistogramBoolean(kPageAttemptsHistogram, true);
  return CONTINUE_OBSERVING;
}

void ServiceLevelPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogPageAttemptEnd(timing, GetDelegate());
}

void ServiceLevelPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  LogPageAttemptEnd(failed_provisional_load_info.discard_reason);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceLevelPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  LogPageAttemptEnd(timing, GetDelegate());
  return STOP_OBSERVING;
}

void ServiceLevelPageLoadMetricsObserver::OnDidInternalNavigationAbort(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNavigationDiscardReason().has_value()) {
    LogPageAttemptEnd(navigation_handle->GetNavigationDiscardReason().value());
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceLevelPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
ServiceLevelPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

void ServiceLevelPageLoadMetricsObserver::LogPageAttemptEnd(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  if (timing.paint_timing->first_contentful_paint.has_value() &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, delegate)) {
    LogPageAttemptEnd(WebViewPageAttemptResult::kSuccess);
    return;
  }

  switch (delegate.GetPageEndReason()) {
    case page_load_metrics::PageEndReason::END_RENDER_PROCESS_GONE:
      LogPageAttemptEnd(WebViewPageAttemptResult::kFailure);
      return;
    default:
      LogPageAttemptEnd(WebViewPageAttemptResult::kEarlyFinish);
      return;
  }
}

void ServiceLevelPageLoadMetricsObserver::LogPageAttemptEnd(
    content::NavigationDiscardReason discard_reason) {
  switch (discard_reason) {
    case content::NavigationDiscardReason::kRenderProcessGone:
      LogPageAttemptEnd(WebViewPageAttemptResult::kFailure);
      return;
    default:
      LogPageAttemptEnd(WebViewPageAttemptResult::kEarlyFinish);
      return;
  }
}

void ServiceLevelPageLoadMetricsObserver::LogPageAttemptEnd(
    WebViewPageAttemptResult result) {
  CHECK(!logged_end_);
  logged_end_ = true;
  base::UmaHistogramEnumeration(kPageAttemptResultsHistogram, result);
}

}  // namespace android_webview
