// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/google/core/common/google_util.h"
#include "components/page_load_metrics/browser/observers/abandoned_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "content/public/browser/navigation_handle_timing.h"

namespace internal {
// Exposed for tests.
extern const char kGWSAbandonedPageLoadMetricsHistogramPrefix[];
extern const char kSuffixWasNonSRP[];

}  // namespace internal

// Observes and records UMA for navigations to GWS which might or might get
// "abandoned" at some point during the navigation / loading. Different from
// AbandonedPageLoadMetricsObserver, this will only observe navigations that
// target GWS (either from the start or after redirections).
class GWSAbandonedPageLoadMetricsObserver
    : public AbandonedPageLoadMetricsObserver {
 public:
  static const char* GetSuffixForRTT(std::optional<base::TimeDelta> rtt);

  GWSAbandonedPageLoadMetricsObserver();
  ~GWSAbandonedPageLoadMetricsObserver() override;

  GWSAbandonedPageLoadMetricsObserver(
      const GWSAbandonedPageLoadMetricsObserver&) = delete;
  GWSAbandonedPageLoadMetricsObserver& operator=(
      const GWSAbandonedPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;

 private:
  // AbandonedPageLoadMetricsObserver overrides:
  std::string GetHistogramPrefix() const override;
  std::vector<std::string> GetAdditionalSuffixes() const override;
  ObservePolicy OnNavigationEvent(
      content::NavigationHandle* navigation_handle) override;
  bool IsAllowedToLogMetrics() const override;
  const base::flat_map<std::string, NavigationMilestone>&
  GetCustomUserTimingMarkNames() const override;
  bool IsAllowedToLogUKM() const override;
  void AddSRPMetricsToUKMIfNeeded(
      ukm::builders::AbandonedSRPNavigation& ukm) override;

  // Set to true if we see the navigation involves non-SRP URL, which will be
  // specially marked in the logged metrics.
  bool did_request_non_srp_ = false;
  // Set to true if we see the navigation involves SRP URL, which means we need
  // to log metrics for this navigation.
  bool involved_srp_url_ = false;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
