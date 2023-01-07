// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FORMFILL_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FORMFILL_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

// This class is responsible for:
// 1. observing heuristically detected fields with filled user data, and storing
// the detection to WebsiteSettings.
// 2. logging metrics if fields with filled user data were previously detected
// on the site.
class FormfillPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  FormfillPageLoadMetricsObserver();
  ~FormfillPageLoadMetricsObserver() override;

  FormfillPageLoadMetricsObserver(const FormfillPageLoadMetricsObserver&) =
      delete;
  FormfillPageLoadMetricsObserver& operator=(
      const FormfillPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver
  const char* GetObserverName() const override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features) override;

  void MaybeRecordPriorUsageOfUserData(
      content::NavigationHandle* navigation_handle);

 private:
  bool user_data_field_detected_ = false;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FORMFILL_PAGE_LOAD_METRICS_OBSERVER_H_
