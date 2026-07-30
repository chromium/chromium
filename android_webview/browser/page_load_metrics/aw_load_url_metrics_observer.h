// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_LOAD_URL_METRICS_OBSERVER_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_LOAD_URL_METRICS_OBSERVER_H_

#include <optional>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace android_webview {

// Records WebView specific metrics measuring the time from the
// AwContents.loadUrl API call to various page load events (e.g. Network Start,
// First Contentful Paint).
class AwLoadUrlMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  AwLoadUrlMetricsObserver() = default;

  AwLoadUrlMetricsObserver(const AwLoadUrlMetricsObserver&) = delete;
  AwLoadUrlMetricsObserver& operator=(const AwLoadUrlMetricsObserver&) = delete;

  ~AwLoadUrlMetricsObserver() override = default;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // The timestamp of the AwContents.loadUrl API call, retrieved from
  // LoadUrlMetricsState.
  std::optional<base::TimeTicks> load_url_timestamp_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_LOAD_URL_METRICS_OBSERVER_H_
