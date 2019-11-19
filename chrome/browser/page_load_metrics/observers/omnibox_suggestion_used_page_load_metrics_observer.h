// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OMNIBOX_SUGGESTION_USED_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OMNIBOX_SUGGESTION_USED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "ui/base/page_transition_types.h"

class OmniboxSuggestionUsedMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit OmniboxSuggestionUsedMetricsObserver(bool is_prerender);
  ~OmniboxSuggestionUsedMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstMeaningfulPaintInMainFrameDocument(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

 private:
  // Indicates whether this page load comes from prerender.
  const bool is_prerender_;
  ui::PageTransition transition_type_ = ui::PAGE_TRANSITION_LINK;

  DISALLOW_COPY_AND_ASSIGN(OmniboxSuggestionUsedMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_OMNIBOX_SUGGESTION_USED_PAGE_LOAD_METRICS_OBSERVER_H_
