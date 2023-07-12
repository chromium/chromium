// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace policy {

// Similar to PolicyTest but sets a couple of policies before the browser is
// started.
class PolicyStatisticsCollectorTest : public PolicyTest {
 public:
  PolicyStatisticsCollectorTest() = default;
  ~PolicyStatisticsCollectorTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
#if !BUILDFLAG(IS_ANDROID)
    policies.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                 nullptr);
    policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);
#endif  // !BUILDFLAG(IS_ANDROID)
    policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value("http://chromium.org"), nullptr);
    provider_.UpdateChromePolicy(policies);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyStatisticsCollectorTest, Startup) {
  // Verifies that policy usage histograms are collected at startup.

  // BrowserPolicyConnector::Init() has already been called. Make sure the
  // CompleteInitialization() task has executed as well.
  content::RunAllPendingInMessageLoop();

  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram("Enterprise.Policies");
  std::unique_ptr<base::HistogramSamples> samples(histogram->SnapshotSamples());
  // HomepageLocation has policy ID 1.
  EXPECT_GT(samples->GetCount(1), 0);
#if !BUILDFLAG(IS_ANDROID)
  // ShowHomeButton has policy ID 35.
  EXPECT_GT(samples->GetCount(35), 0);
  // BookmarkBarEnabled has policy ID 82.
  EXPECT_GT(samples->GetCount(82), 0);
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace policy
