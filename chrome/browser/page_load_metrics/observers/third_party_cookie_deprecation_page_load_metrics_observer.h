// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_COOKIE_DEPRECATION_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_COOKIE_DEPRECATION_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace tpcd::experiment {
class ExperimentManager;
}  // namespace tpcd::experiment

// ThirdPartyCookieDeprecationMetricsObserver is responsible for recording
// number of page load sends at least one third party cookie while the
// experiment configuration is set to block third party cookies.
class ThirdPartyCookieDeprecationMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  explicit ThirdPartyCookieDeprecationMetricsObserver(
      content::BrowserContext* context);

  ThirdPartyCookieDeprecationMetricsObserver(
      const ThirdPartyCookieDeprecationMetricsObserver&) = delete;
  ThirdPartyCookieDeprecationMetricsObserver& operator=(
      const ThirdPartyCookieDeprecationMetricsObserver&) = delete;

  ~ThirdPartyCookieDeprecationMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;

  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     bool blocked_by_policy) override;

  void OnCookieChange(const GURL& url,
                      const GURL& first_party_url,
                      const net::CanonicalCookie& cookie,
                      bool blocked_by_policy) override;

 private:
  // Records feature cookie access metric.
  void RecordCookieUseCounters(const GURL& url,
                               const GURL& first_party_url,
                               bool blocked_by_policy);

  // Returns whether the two inputs |url| and |first_party_url| are third party
  // one another.
  bool IsThirdParty(const GURL& url, const GURL& first_party_url);

  bool IsBlockedByThirdPartyDeprecationExperiment();

  // Not owned and the lifetime of ExperimentManager will exceed |this|.
  raw_ptr<tpcd::experiment::ExperimentManager> experiment_manager_;
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_THIRD_PARTY_COOKIE_DEPRECATION_PAGE_LOAD_METRICS_OBSERVER_H_
