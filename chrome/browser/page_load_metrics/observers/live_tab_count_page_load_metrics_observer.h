// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LIVE_TAB_COUNT_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LIVE_TAB_COUNT_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

// Exposed for tests.
extern const char kHistogramPrefixLiveTabCount[];

}  // namespace internal

// Observer responsible for recording core page load metrics bucketed by the
// number of live tabs.
class LiveTabCountPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  LiveTabCountPageLoadMetricsObserver();
  ~LiveTabCountPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 protected:
  // Returns the number of live tabs, including the one that we're observing.
  // This is virtual and protected so we can control the live tab count from
  // unit tests.
  virtual size_t GetLiveTabCount() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveTabCountPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_LIVE_TAB_COUNT_PAGE_LOAD_METRICS_OBSERVER_H_
