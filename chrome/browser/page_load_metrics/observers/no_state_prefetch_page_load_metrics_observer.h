// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NO_STATE_PREFETCH_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NO_STATE_PREFETCH_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace prerender {
class PrerenderManager;
}

// Observer responsible for recording First Contentful Paint metrics related to
// NoStatePrefetch.
//
// This observer should be attached to all WebContents instances that are _not_
// being prerendered. For prerendered page loads, analagous metrics are recorded
// via |PrerenderPageLoadMetricsObserver|. This allows to compare FCP metrics
// between three mechanisms: Prerender, NoStatePrefetch, Noop.
//
// To record the histograms the knowledge of this class is combined with
// information from |PrerenderManager|:
//   * the kind of prefetch, i.e.: prerender::Origin
//   * whether the load was eligible for prefetch/prerender (also how long ago)
class NoStatePrefetchPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  // Returns a NoStatePrefetchPageLoadMetricsObserver, or nullptr if it is not
  // needed. Note: can return nullptr at startup, which is believed to be
  // happening rarely.
  static std::unique_ptr<NoStatePrefetchPageLoadMetricsObserver> CreateIfNeeded(
      content::WebContents* web_contents);

  explicit NoStatePrefetchPageLoadMetricsObserver(
      prerender::PrerenderManager* manager);
  ~NoStatePrefetchPageLoadMetricsObserver() override;

 private:
  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  bool is_no_store_;  // True if the main resource has a "no-store" HTTP header.
  bool was_hidden_;   // The page went to background while rendering.
  prerender::PrerenderManager* const prerender_manager_;

  DISALLOW_COPY_AND_ASSIGN(NoStatePrefetchPageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_NO_STATE_PREFETCH_PAGE_LOAD_METRICS_OBSERVER_H_
