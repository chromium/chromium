// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/page_load_metrics/observers/https_engagement_metrics/https_engagement_service.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace internal {
extern const char kHttpsEngagementHistogram[];
extern const char kHttpEngagementHistogram[];
}  // namespace internal

class HttpsEngagementPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit HttpsEngagementPageLoadMetricsObserver(
      content::BrowserContext* context);

  HttpsEngagementPageLoadMetricsObserver(
      const HttpsEngagementPageLoadMetricsObserver&) = delete;
  HttpsEngagementPageLoadMetricsObserver& operator=(
      const HttpsEngagementPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  raw_ptr<HttpsEngagementService> engagement_service_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_HTTPS_ENGAGEMENT_METRICS_HTTPS_ENGAGEMENT_PAGE_LOAD_METRICS_OBSERVER_H_
