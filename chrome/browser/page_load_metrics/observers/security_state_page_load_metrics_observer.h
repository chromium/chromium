// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SECURITY_STATE_PAGE_LOAD_METRICS_OBSERVER_H_
#define CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SECURITY_STATE_PAGE_LOAD_METRICS_OBSERVER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class BrowserContext;
}  // namespace content

class SecurityStateTabHelper;
class SiteEngagementService;

// Tracks the SecurityLevel of the page from the time it commits to the time it
// completes. This is uses to track metrics keyed on the SecurityLevel of the
// page. This has the same lifetime as a traditional PageLoadMetricsObserver,
// not a WebContentsObserver.
class SecurityStatePageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver,
      public content::WebContentsObserver {
 public:
  // Create a SecurityStatePageLoadMetricsObserver using the profile's
  // SiteEngagementService, if it exists. Otherwise, creates a
  // SecurityStatePageLoadMetricsObserver that will not track site engagement
  // metrics.
  static std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
  MaybeCreateForProfile(content::BrowserContext* profile);

  static std::string GetEngagementDeltaHistogramNameForTesting(
      security_state::SecurityLevel level);
  static std::string GetEngagementFinalHistogramNameForTesting(
      security_state::SecurityLevel level);
  static std::string GetSecurityLevelPageEndReasonHistogramNameForTesting(
      security_state::SecurityLevel level);
  static std::string GetSafetyTipPageEndReasonHistogramNameForTesting(
      security_state::SafetyTipStatus safety_tip_status);

  explicit SecurityStatePageLoadMetricsObserver(
      SiteEngagementService* engagement_service);
  ~SecurityStatePageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // content::WebContentsObserver:
  void DidChangeVisibleSecurityState() override;

 private:
  // If the SiteEngagementService does not exist, this will be null.
  SiteEngagementService* engagement_service_ = nullptr;

  SecurityStateTabHelper* security_state_tab_helper_ = nullptr;
  double initial_engagement_score_ = 0.0;
  security_state::SecurityLevel initial_security_level_ = security_state::NONE;
  security_state::SecurityLevel current_security_level_ = security_state::NONE;
  ukm::SourceId source_id_ = ukm::kInvalidSourceId;

  DISALLOW_COPY_AND_ASSIGN(SecurityStatePageLoadMetricsObserver);
};

#endif  // CHROME_BROWSER_PAGE_LOAD_METRICS_OBSERVERS_SECURITY_STATE_PAGE_LOAD_METRICS_OBSERVER_H_
