// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service_factory.h"
#include "url/url_constants.h"

namespace internal {
const char kHttpsEngagementHistogram[] = "Navigation.EngagementTime.HTTPS";
const char kHttpEngagementHistogram[] = "Navigation.EngagementTime.HTTP";
}

HttpsEngagementPageLoadMetricsObserver::HttpsEngagementPageLoadMetricsObserver(
    content::BrowserContext* context) {
  engagement_service_ =
      HttpsEngagementServiceFactory::GetForBrowserContext(context);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
HttpsEngagementPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // This class is interested only in the primary page's end of life timing,
  // and doesn't need to continue observing FencedFrame pages.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
HttpsEngagementPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Once prerendered pages have been activated we will want to report this
  // metric. Statistics are only be reported when foreground_time is nonzero so
  // there are no additional checks needed.
  return CONTINUE_OBSERVING;
}

void HttpsEngagementPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!GetDelegate().DidCommit() || !GetDelegate().GetUrl().is_valid()) {
    return;
  }

  // Don't record anything if the user never saw it.
  base::TimeDelta foreground_time =
      GetDelegate().GetVisibilityTracker().GetForegroundDuration();
  if (foreground_time.is_zero())
    return;

  if (GetDelegate().GetUrl().SchemeIs(url::kHttpsScheme)) {
    if (engagement_service_) {
      engagement_service_->RecordTimeOnPage(foreground_time,
                                            HttpsEngagementService::HTTPS);
    }
    UMA_HISTOGRAM_LONG_TIMES_100(internal::kHttpsEngagementHistogram,
                                 foreground_time);
  } else if (GetDelegate().GetUrl().SchemeIs(url::kHttpScheme)) {
    if (engagement_service_) {
      engagement_service_->RecordTimeOnPage(foreground_time,
                                            HttpsEngagementService::HTTP);
    }
    UMA_HISTOGRAM_LONG_TIMES_100(internal::kHttpEngagementHistogram,
                                 foreground_time);
  }
}
