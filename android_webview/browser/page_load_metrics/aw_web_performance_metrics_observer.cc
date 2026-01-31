// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_web_performance_metrics_observer.h"

#include "android_webview/browser/aw_contents.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

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

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
AwWebPerformanceMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void AwWebPerformanceMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (aw_contents) {
    aw_contents->GetNavigationClient()->OnFirstContentfulPaint(
        web_contents->GetPrimaryPage(),
        timing.paint_timing->first_contentful_paint.value());
  }
}

void AwWebPerformanceMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Buffered metrics aren't available for navigations that do not commit
  // or haven't committed yet.
  if (!GetDelegate().DidCommit()) {
    return;
  }

  // Report largest contentful paint.
  // Note: lcp is tracked separately for images and text, but we only want to
  // report the largest paint overall. Below we compare the values from image
  // lcp and text lcp, to see which is the largest overall. Additionally
  // OnTimingUpdate is triggered when an updated PageLoadTiming is available at
  // the page. It can be called multiple times over the course of the page load
  // and not just when lcp is updated. Hence we also compare the values to the
  // last lcp reported to determine whether lcp has been updated.
  page_load_metrics::mojom::LargestContentfulPaintTimingPtr lcp_timing =
      std::move(timing.paint_timing->largest_contentful_paint);
  uint64_t lcp_image_size = lcp_timing->largest_image_paint_size;
  std::optional<base::TimeDelta> lcp_image_duration =
      lcp_timing->largest_image_paint;
  uint64_t lcp_text_size = lcp_timing->largest_text_paint_size;
  std::optional<base::TimeDelta> lcp_text_duration =
      lcp_timing->largest_text_paint;
  std::optional<base::TimeDelta> new_lcp;

  if (lcp_image_size > lcp_largest_reported_size_ ||
      lcp_text_size > lcp_largest_reported_size_) {
    if (lcp_image_size > lcp_text_size) {
      CHECK(lcp_image_duration);
      new_lcp = lcp_image_duration;
      lcp_largest_reported_size_ = lcp_image_size;
    } else {
      CHECK(lcp_text_duration);
      new_lcp = lcp_text_duration;
      lcp_largest_reported_size_ = lcp_text_size;
    }

    content::WebContents* web_contents = GetDelegate().GetWebContents();
    AwContents* aw_contents = AwContents::FromWebContents(web_contents);
    if (aw_contents) {
      aw_contents->GetNavigationClient()->OnLargestContentfulPaint(
          web_contents->GetPrimaryPage(), new_lcp.value());
    }
  }
}

void AwWebPerformanceMetricsObserver::OnUserTimingMarkFullyLoaded(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO: crbug.com/461774316 - Clean up check
  DCHECK(timing.user_timing_mark_fully_loaded);
  if (!timing.user_timing_mark_fully_loaded) {
    return;
  }
  SendPerformanceMark(kMarkFullyLoaded,
                      timing.user_timing_mark_fully_loaded.value());
}

void AwWebPerformanceMetricsObserver::OnUserTimingMarkFullyVisible(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO: crbug.com/461774316 - Clean up check
  DCHECK(timing.user_timing_mark_fully_visible);
  if (!timing.user_timing_mark_fully_visible) {
    return;
  }
  SendPerformanceMark(kMarkFullyVisible,
                      timing.user_timing_mark_fully_visible.value());
}

void AwWebPerformanceMetricsObserver::OnUserTimingMarkInteractive(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // TODO: crbug.com/461774316 - Clean up check
  DCHECK(timing.user_timing_mark_interactive);
  if (!timing.user_timing_mark_interactive) {
    return;
  }
  SendPerformanceMark(kMarkInteractive,
                      timing.user_timing_mark_interactive.value());
}

void AwWebPerformanceMetricsObserver::SendPerformanceMark(
    std::string mark_name,
    const base::TimeDelta& mark_time) {
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (aw_contents) {
    aw_contents->GetNavigationClient()->OnPerformanceMark(
        web_contents->GetPrimaryPage(), mark_name, mark_time);
  }
}

void AwWebPerformanceMetricsObserver::OnCustomUserTimingMarkObserved(
    const std::vector<page_load_metrics::mojom::CustomUserTimingMarkPtr>&
        timings) {
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (aw_contents) {
    content::Page& page = web_contents->GetPrimaryPage();
    for (const auto& mark : timings) {
      aw_contents->GetNavigationClient()->OnPerformanceMark(
          page, mark->mark_name, mark->start_time);
    }
  }
}
}  // namespace android_webview
