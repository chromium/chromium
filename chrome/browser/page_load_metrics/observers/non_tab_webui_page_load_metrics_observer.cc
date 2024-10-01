// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/non_tab_webui_page_load_metrics_observer.h"

#include <optional>

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/common/url_constants.h"

constexpr char kNonTabWebUINavigationToLCPHistogramName[] =
    "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.NonTabWebUI";

constexpr char kNonTabWebUINavigationToFCPHistogramName[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NonTabWebUI";

constexpr char kNonTabWebUIRequestToLCPHistogramName[] =
    "WebUI.TopChrome.RequestToLCP";

constexpr char kNonTabWebUIRequestToFCPHistogramName[] =
    "WebUI.TopChrome.RequestToFCP";

namespace {

std::string GetSuffixedLCPHistogram(std::string_view webui_name) {
  return base::StrCat(
      {kNonTabWebUINavigationToLCPHistogramName, ".", webui_name});
}

std::string GetSuffixedFCPHistogram(std::string_view webui_name) {
  return base::StrCat(
      {kNonTabWebUINavigationToFCPHistogramName, ".", webui_name});
}

std::string GetSuffixedRequestToLCPHistogram(std::string_view webui_name) {
  return base::StrCat({kNonTabWebUIRequestToLCPHistogramName, ".", webui_name});
}

std::string GetSuffixedRequestToFCPHistogram(std::string_view webui_name) {
  return base::StrCat({kNonTabWebUIRequestToFCPHistogramName, ".", webui_name});
}

// We define "background time" as the amount of time between navigation and
// when user opens the WebUI. When a WebUI is preloaded, navigation can happen
// well before the user opens the WebUI.
base::TimeDelta GetBackgroundTime(
    const page_load_metrics::PageLoadMetricsObserverDelegate& delegate) {
  const std::optional<base::TimeTicks> request_time =
      WebUIContentsPreloadManager::GetInstance()->GetRequestTime(
          delegate.GetWebContents());
  if (!request_time.has_value()) {
    // The WebUIContentsPreloadManager may not have a record of when the user
    // opened the WebUI. This may happen in unit tests, or if a non-tab WebUI
    // is opened in a tab for debugging purposes. In these cases, we define the
    // "background time" to be zero.
    return base::TimeDelta();
  }

  const base::TimeTicks last_navigation_time = delegate.GetNavigationStart();
  // The request time is earlier than the last navigation time if the WebUI
  // refreshes or redirects. In this case the WebUI is never in the background
  // since last navigation.
  const base::TimeDelta background_time =
      std::max(*request_time - last_navigation_time, base::TimeDelta());
  return background_time;
}

}  // namespace

NonTabPageLoadMetricsObserver::NonTabPageLoadMetricsObserver(
    const std::string& webui_name)
    : page_load_metrics::PageLoadMetricsObserver(), webui_name_(webui_name) {
  base::trace_event::EmitNamedTrigger("non-tab-webui-creation");
}

void NonTabPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint.has_value());

  base::TimeDelta first_contentful_paint =
      timing.paint_timing->first_contentful_paint.value();
  // Time from navigation to LCP and FCP. They can be very large for preloaded
  // WebUIs because the LCP and FCP are not recorded until the WebUI is actually
  // shown.
  PAGE_LOAD_HISTOGRAM(kNonTabWebUINavigationToFCPHistogramName,
                      first_contentful_paint);
  PAGE_LOAD_HISTOGRAM(GetSuffixedFCPHistogram(webui_name_),
                      first_contentful_paint);

  // Time from request to FCP. This metric disregards time spent in the
  // background, which is non-zero when the WebUI is preloaded.
  base::TimeDelta background_time = GetBackgroundTime(GetDelegate());
  PAGE_LOAD_SHORT_HISTOGRAM(kNonTabWebUIRequestToFCPHistogramName,
                            first_contentful_paint - background_time);
  PAGE_LOAD_SHORT_HISTOGRAM(GetSuffixedRequestToFCPHistogram(webui_name_),
                            first_contentful_paint - background_time);
}

void NonTabPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MainFrameLargestContentfulPaint();
  // It's possible to get here and for LCP timing to not be available.
  if (!main_frame_largest_contentful_paint.ContainsValidTime()) {
    return;
  }
  PAGE_LOAD_HISTOGRAM(kNonTabWebUINavigationToLCPHistogramName,
                      main_frame_largest_contentful_paint.Time().value());
  PAGE_LOAD_HISTOGRAM(GetSuffixedLCPHistogram(webui_name_),
                      main_frame_largest_contentful_paint.Time().value());

  // Time from request to LCP. This metric disregards time spent in the
  // background, which is non-zero when the WebUI is preloaded.
  base::TimeDelta background_time = GetBackgroundTime(GetDelegate());
  PAGE_LOAD_SHORT_HISTOGRAM(
      kNonTabWebUIRequestToLCPHistogramName,
      main_frame_largest_contentful_paint.Time().value() - background_time);
  PAGE_LOAD_SHORT_HISTOGRAM(
      GetSuffixedRequestToLCPHistogram(webui_name_),
      main_frame_largest_contentful_paint.Time().value() - background_time);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NonTabPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NonTabPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NonTabPageLoadMetricsObserver::ShouldObserveScheme(const GURL& url) const {
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}
