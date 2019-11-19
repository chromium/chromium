// Copyright 2016 The Chromium Authors. All rights reserved.
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
