// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LOADING_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LOADING_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace content {
class WebContents;
}

namespace predictors {
class ResourcePrefetchPredictor;
class LoadingDataCollector;
}

namespace internal {

extern const char
    kHistogramLoadingPredictorFirstContentfulPaintPreconnectable[];
extern const char
    kHistogramLoadingPredictorFirstMeaningfulPaintPreconnectable[];

}  // namespace internal

// Observer responsible for recording page load metrics relevant to
// ResourcePrefetchPredictor.
class LoadingPredictorPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a LoadingPredictorPageLoadMetricsObserver, or nullptr if it is not
  // needed.
  static std::unique_ptr<LoadingPredictorPageLoadMetricsObserver>
  CreateIfNeeded(content::WebContents* web_contents);

  // Public for testing. Normally one should use CreateIfNeeded. Predictor must
  // outlive this observer.
  explicit LoadingPredictorPageLoadMetricsObserver(
      predictors::ResourcePrefetchPredictor* predictor,
      predictors::LoadingDataCollector* collector);

  ~LoadingPredictorPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_commited_url,
                        bool started_in_foreground) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  predictors::ResourcePrefetchPredictor* predictor_;
  predictors::LoadingDataCollector* collector_;
  bool record_histogram_preconnectable_;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictorPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LOADING_PREDICTOR_PAGE_LOAD_METRICS_OBSERVER_H_
