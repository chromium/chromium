// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/welcome/helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

// Tests that the PromotionalTabsEnabled policy properly suppresses the welcome
// page for browser first-runs.
class PromotionalTabsEnabledPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<PolicyTest::BooleanPolicy> {
 public:
  PromotionalTabsEnabledPolicyTest(const PromotionalTabsEnabledPolicyTest&) =
      delete;
  PromotionalTabsEnabledPolicyTest& operator=(
      const PromotionalTabsEnabledPolicyTest&) = delete;

 protected:
  PromotionalTabsEnabledPolicyTest() {
    scoped_feature_list_.InitWithFeatures({welcome::kForceEnabled}, {});
  }
  ~PromotionalTabsEnabledPolicyTest() override = default;

  void SetUp() override {
    // Ordinarily, browser tests include chrome://blank on the command line to
    // suppress any onboarding or promotional tabs. This test, on the other
    // hand, must evaluate startup with nothing on the command line so that a
    // default launch takes place.
    set_open_about_blank_on_browser_launch(false);
    PolicyTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceFirstRun);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    // Set policies before the browser starts up.
    PolicyMap policies;

    // Suppress the first-run dialog by disabling metrics reporting.
    policies.Set(key::kMetricsReportingEnabled, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(false),
                 nullptr);

    // Apply the policy setting under test.
    if (GetParam() != BooleanPolicy::kNotConfigured) {
      policies.Set(key::kPromotionalTabsEnabled, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value(GetParam() == BooleanPolicy::kTrue), nullptr);
    }

    UpdateProviderPolicy(policies);
    PolicyTest::CreatedBrowserMainParts(browser_main_parts);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PromotionalTabsEnabledPolicyTest, RunTest) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_GE(tab_strip->count(), 1);
  const auto& url = tab_strip->GetWebContentsAt(0)->GetURL();
  switch (GetParam()) {
    case BooleanPolicy::kFalse:
      // Only the NTP should show.
      EXPECT_EQ(tab_strip->count(), 1);
      if (url.possibly_invalid_spec() != chrome::kChromeUINewTabURL)
        EXPECT_TRUE(search::IsNTPOrRelatedURL(url, browser()->profile()))
            << url;
      break;
    case BooleanPolicy::kNotConfigured:
    case BooleanPolicy::kTrue:
      // One or more onboarding tabs should show.
      EXPECT_NE(url.possibly_invalid_spec(), chrome::kChromeUINewTabURL);
      EXPECT_FALSE(search::IsNTPOrRelatedURL(url, browser()->profile())) << url;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PromotionalTabsEnabledPolicyTest,
    ::testing::Values(PolicyTest::BooleanPolicy::kNotConfigured,
                      PolicyTest::BooleanPolicy::kFalse,
                      PolicyTest::BooleanPolicy::kTrue));

}  // namespace policy
