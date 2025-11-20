// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_web_performance_metrics_observer.h"

#include "android_webview/browser/aw_contents.h"

namespace android_webview {

const char* AwWebPerformanceMetricsObserver::GetObserverName() const {
  static const char kName[] = "AwWebPerformanceMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwWebPerformanceMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwWebPerformanceMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Fenced frames are not supported in WebView
  return STOP_OBSERVING;
}

void AwWebPerformanceMetricsObserver::OnUserTimingMarkFullyLoaded(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO: crbug.com/461774316 - Clean up check
  DCHECK(timing.user_timing_mark_fully_loaded);
  if (!timing.user_timing_mark_fully_loaded) {
    return;
  }
  AwContents::FromWebContents(GetDelegate().GetWebContents())
      ->OnPerformanceMark(kMarkFullyLoaded,
                          timing.user_timing_mark_fully_loaded.value());
}

void AwWebPerformanceMetricsObserver::OnUserTimingMarkFullyVisible(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO: crbug.com/461774316 - Clean up check
  DCHECK(timing.user_timing_mark_fully_visible);
  if (!timing.user_timing_mark_fully_visible) {
    return;
  }
  AwContents::FromWebContents(GetDelegate().GetWebContents())
      ->OnPerformanceMark(kMarkFullyVisible,
                          timing.user_timing_mark_fully_visible.value());
}

void AwWebPerformanceMetricsObserver::OnUserTimingMarkInteractive(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO: crbug.com/461774316 - Clean up check
  DCHECK(timing.user_timing_mark_interactive);
  if (!timing.user_timing_mark_interactive) {
    return;
  }
  AwContents::FromWebContents(GetDelegate().GetWebContents())
      ->OnPerformanceMark(kMarkInteractive,
                          timing.user_timing_mark_interactive.value());
}

void AwWebPerformanceMetricsObserver::OnCustomUserTimingMarkObserved(
    const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
        timings) {
  AwContents* aw_contents =
      AwContents::FromWebContents(GetDelegate().GetWebContents());
  for (const auto& mark : timings) {
    aw_contents->OnPerformanceMark(mark->mark_name, mark->start_time);
  }
}

}  // namespace android_webview
