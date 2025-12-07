// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/new_tab_page_page_load_metrics_observer.h"

#include "base/notreached.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"

namespace {

const char kNewTabPageFirstContentfulPaintHistogram[] =
    "NewTabPage.LoadTime.FirstContentfulPaint";
const char kNewTabPageLargestContentfulPaintHistogram[] =
    "NewTabPage.LoadTime.LargestContentfulPaint";

}  // namespace

NewTabPagePageLoadMetricsObserver::NewTabPagePageLoadMetricsObserver() =
    default;

NewTabPagePageLoadMetricsObserver::~NewTabPagePageLoadMetricsObserver() =
    default;

const char* NewTabPagePageLoadMetricsObserver::GetObserverName() const {
  return "NewTabPagePageLoadMetricsObserver";
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPagePageLoadMetricsObserver::ShouldObserveScheme(const GURL& url) const {
  // Observes chrome:// and chrome-untrusted://, which are not observed by
  // default.
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeUIUntrustedScheme)) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPagePageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  // Observes chrome://new-tab-page and chrome-untrusted://new-tab-page
  // but not third-party NTPs.
  if (url::IsSameOriginWith(navigation_handle->GetURL(),
                            GURL(chrome::kChromeUINewTabPageURL)) ||
      url::IsSameOriginWith(navigation_handle->GetURL(),
                            GURL(chrome::kChromeUIUntrustedNewTabPageUrl))) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPagePageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Prerenderer should never run on non-HTTPs navigations.
  NOTREACHED();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPagePageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in the primary page.
  return STOP_OBSERVING;
}

void NewTabPagePageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(timing.paint_timing->first_contentful_paint);
  PAGE_LOAD_HISTOGRAM(kNewTabPageFirstContentfulPaintHistogram,
                      *timing.paint_timing->first_contentful_paint);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
NewTabPagePageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (GetDelegate().DidCommit()) {
    RecordSessionEndHistograms(timing);
  }
  return STOP_OBSERVING;
}

void NewTabPagePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

void NewTabPagePageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  CHECK(GetDelegate().DidCommit());
  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();

  if (!largest_contentful_paint.ContainsValidTime()) {
    return;
  }

  PAGE_LOAD_HISTOGRAM(kNewTabPageLargestContentfulPaintHistogram,
                      largest_contentful_paint.Time().value());
}
