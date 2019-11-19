// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OFFLINE_PAGE_PREVIEWS_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OFFLINE_PAGE_PREVIEWS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace previews {

namespace internal {

// Various UMA histogram names for Previews core page load metrics.
extern const char kHistogramOfflinePreviewsDOMContentLoadedEventFired[];
extern const char kHistogramOfflinePreviewsFirstLayout[];
extern const char kHistogramOfflinePreviewsLoadEventFired[];
extern const char kHistogramOfflinePreviewsFirstContentfulPaint[];
extern const char kHistogramOfflinePreviewsParseStart[];
extern const char kHistogramOfflinePreviewsPageEndReason[];

}  // namespace internal

// Observer responsible for recording core page load metrics relevant to
// Previews.
class OfflinePagePreviewsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  OfflinePagePreviewsPageLoadMetricsObserver();
  ~OfflinePagePreviewsPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstLayout(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnParseStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

 private:
  // Whether |web_contents| is showing an offline pages preview. Overridden in
  // testing.
  virtual bool IsOfflinePreview(content::WebContents* web_contents) const;

  // Records metrics derived from global page load state.
  void RecordPageLoadMetrics();

  DISALLOW_COPY_AND_ASSIGN(OfflinePagePreviewsPageLoadMetricsObserver);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OFFLINE_PAGE_PREVIEWS_PAGE_LOAD_METRICS_OBSERVER_H_
