// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/security_state_page_load_metrics_observer.h"

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ssl/tls_deprecation_config.h"
#include "chrome/browser/ssl/tls_deprecation_config.pb.h"
#include "chrome/browser/ssl/tls_deprecation_test_utils.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "ui/base/scoped_visibility_tracker.h"

class SecurityStatePageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  SecurityStatePageLoadMetricsObserverTest()
      : page_load_metrics::PageLoadMetricsObserverTestHarness() {
    clock_ = std::make_unique<base::SimpleTestTickClock>();
    clock_->SetNowTicks(base::TimeTicks::Now());
  }
  ~SecurityStatePageLoadMetricsObserverTest() override {}

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
    SecurityStateTabHelper::CreateForWebContents(web_contents());
    helper_ = SecurityStateTabHelper::FromWebContents(web_contents());
  }

  void AdvancePageDuration(base::TimeDelta delta) { clock_->Advance(delta); }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    // Currently does not support testing Site Engagement metrics.
    tracker->AddObserver(
        std::make_unique<SecurityStatePageLoadMetricsObserver>(nullptr));

    // Swap out the ui::ScopedVisibilityTracker to use the test clock.
    ui::ScopedVisibilityTracker visibility_tracker(clock_.get(), true);
    tracker->SetVisibilityTrackerForTesting(visibility_tracker);
  }

  SecurityStateTabHelper* security_state_tab_helper() { return helper_; }

 private:
  SecurityStateTabHelper* helper_;

  // The clock used by the ui::ScopedVisibilityTracker.
  std::unique_ptr<base::SimpleTestTickClock> clock_;

  DISALLOW_COPY_AND_ASSIGN(SecurityStatePageLoadMetricsObserverTest);
};

TEST_F(SecurityStatePageLoadMetricsObserverTest, LegacyTLSMetrics) {
  InitializeEmptyLegacyTLSConfig();

  auto navigation =
      CreateLegacyTLSNavigation(GURL(kLegacyTLSDefaultURL), web_contents());
  navigation->Commit();

  tester()->histogram_tester().ExpectBucketCount("Security.LegacyTLS.OnCommit",
                                                 true, 1);

  const base::TimeDelta kMinForegroundTime =
      base::TimeDelta::FromMilliseconds(10);
  AdvancePageDuration(kMinForegroundTime);

  navigation->Reload(web_contents());

  tester()->histogram_tester().ExpectBucketCount(
      "Security.PageEndReason.LegacyTLS_Triggered",
      page_load_metrics::END_RELOAD, 1);

  auto samples = tester()->histogram_tester().GetAllSamples(
      "Security.TimeOnPage2.LegacyTLS_Triggered");
  EXPECT_EQ(1u, samples.size());
  EXPECT_LE(kMinForegroundTime.InMilliseconds(), samples.front().min);
}

TEST_F(SecurityStatePageLoadMetricsObserverTest, LegacyTLSControlSiteMetrics) {
  InitializeLegacyTLSConfigWithControl();

  auto navigation =
      CreateLegacyTLSNavigation(GURL(kLegacyTLSControlURL), web_contents());
  navigation->Commit();

  tester()->histogram_tester().ExpectBucketCount("Security.LegacyTLS.OnCommit",
                                                 false, 1);

  const base::TimeDelta kMinForegroundTime =
      base::TimeDelta::FromMilliseconds(10);
  AdvancePageDuration(kMinForegroundTime);

  navigation->Reload(web_contents());

  tester()->histogram_tester().ExpectBucketCount(
      "Security.PageEndReason.LegacyTLS_NotTriggered",
      page_load_metrics::END_RELOAD, 1);

  auto samples = tester()->histogram_tester().GetAllSamples(
      "Security.TimeOnPage2.LegacyTLS_NotTriggered");
  EXPECT_EQ(1u, samples.size());
  EXPECT_LE(kMinForegroundTime.InMilliseconds(), samples.front().min);
}

TEST_F(SecurityStatePageLoadMetricsObserverTest, LegacyTLSGoodSiteMetrics) {
  InitializeEmptyLegacyTLSConfig();

  auto navigation =
      CreateNonlegacyTLSNavigation(GURL("https://good.test"), web_contents());
  navigation->Commit();

  tester()->histogram_tester().ExpectBucketCount("Security.LegacyTLS.OnCommit",
                                                 false, 1);

  const base::TimeDelta kMinForegroundTime =
      base::TimeDelta::FromMilliseconds(10);
  AdvancePageDuration(kMinForegroundTime);

  navigation->Reload(web_contents());

  tester()->histogram_tester().ExpectBucketCount(
      "Security.PageEndReason.LegacyTLS_NotTriggered",
      page_load_metrics::END_RELOAD, 1);

  auto samples = tester()->histogram_tester().GetAllSamples(
      "Security.TimeOnPage2.LegacyTLS_NotTriggered");
  EXPECT_EQ(1u, samples.size());
  EXPECT_LE(kMinForegroundTime.InMilliseconds(), samples.front().min);
}
