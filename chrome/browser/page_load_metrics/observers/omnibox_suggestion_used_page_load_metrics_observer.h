// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OMNIBOX_SUGGESTION_USED_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OMNIBOX_SUGGESTION_USED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/page_transition_types.h"

class OmniboxSuggestionUsedMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  OmniboxSuggestionUsedMetricsObserver();

  OmniboxSuggestionUsedMetricsObserver(
      const OmniboxSuggestionUsedMetricsObserver&) = delete;
  OmniboxSuggestionUsedMetricsObserver& operator=(
      const OmniboxSuggestionUsedMetricsObserver&) = delete;

  ~OmniboxSuggestionUsedMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& timing);

  ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_LINK;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OMNIBOX_SUGGESTION_USED_PAGE_LOAD_METRICS_OBSERVER_H_
