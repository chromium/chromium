// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CHROME_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CHROME_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_

#include "components/page_load_metrics/google/browser/gws_abandoned_page_load_metrics_observer.h"

namespace internal {
extern const char kSuffixResponseFromCache[];
}

// Similar to GWSAbandonedPageLoadMetricsObserver but adds suffixes that are
// only available on //chrome (e.g. connection RTT info).
class ChromeGWSAbandonedPageLoadMetricsObserver
    : public GWSAbandonedPageLoadMetricsObserver {
 public:
  static const char* GetSuffixForRTT(std::optional<base::TimeDelta> rtt);

  ChromeGWSAbandonedPageLoadMetricsObserver();
  ~ChromeGWSAbandonedPageLoadMetricsObserver() override;

  ChromeGWSAbandonedPageLoadMetricsObserver(
      const ChromeGWSAbandonedPageLoadMetricsObserver&) = delete;
  ChromeGWSAbandonedPageLoadMetricsObserver& operator=(
      const ChromeGWSAbandonedPageLoadMetricsObserver&) = delete;

 private:
  std::vector<std::string> GetAdditionalSuffixes() const override;
  void AddSRPMetricsToUKMIfNeeded(
      ukm::builders::AbandonedSRPNavigation& ukm) override;
  bool IsIncognitoProfile() const;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_CHROME_GWS_ABANDONED_PAGE_LOAD_METRICS_OBSERVER_H_
