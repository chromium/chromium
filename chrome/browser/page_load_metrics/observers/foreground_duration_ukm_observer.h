// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FOREGROUND_DURATION_UKM_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FOREGROUND_DURATION_UKM_OBSERVER_H_

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

// Observer responsible for appending previews information to the PLM UKM
// report.
class ForegroundDurationUKMObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  ForegroundDurationUKMObserver();

  ForegroundDurationUKMObserver(const ForegroundDurationUKMObserver&) = delete;
  ForegroundDurationUKMObserver& operator=(
      const ForegroundDurationUKMObserver&) = delete;

  ~ForegroundDurationUKMObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnHidden(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnShown() override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;

 private:
  // True when the visibility of WebContents is Visibility::VISIBLE (not
  // OCCLUDED or HIDDEN).
  bool currently_in_foreground_ = false;

  base::TimeTicks last_time_shown_;
  page_load_metrics::mojom::InputTimingPtr last_page_input_timing_;
  void RecordUkmIfInForeground(base::TimeTicks end_time);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_FOREGROUND_DURATION_UKM_OBSERVER_H_
