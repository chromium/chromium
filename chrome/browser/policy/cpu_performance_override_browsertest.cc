// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"

namespace policy {

class CpuPerformancePolicyTest : public PolicyTest {
 public:
  CpuPerformancePolicyTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kCpuPerformance},
        /*disabled_features=*/{features::kSpareRendererForSitePerProcess});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    content::IsolateAllSitesForTesting(command_line);
  }

 protected:
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Establish the baseline hardware tier via JS before any policies are set.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
    hardware_tier_ = GetCpuPerformanceFromJs(browser());
    CHECK_LE(1, hardware_tier_);
    CHECK_GE(4, hardware_tier_);
    // Calculate a different tier for the policy override.
    policy_override_tier_ = 1 + hardware_tier_ % 4;
    CHECK_LE(1, policy_override_tier_);
    CHECK_GE(4, policy_override_tier_);
    CHECK_NE(hardware_tier_, policy_override_tier_);
    // Calculate yet another different tier for the user override.
    user_override_tier_ = 1 + policy_override_tier_ % 4;
    CHECK_LE(1, user_override_tier_);
    CHECK_GE(4, user_override_tier_);
    CHECK_NE(hardware_tier_, user_override_tier_);
    CHECK_NE(user_override_tier_, policy_override_tier_);
  }

  void SetPolicy(int override_value) {
    PolicyMap policies;
    policies.Set(key::kCpuPerformanceTierOverride, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(override_value), nullptr);
    UpdateProviderPolicy(policies);
    base::RunLoop().RunUntilIdle();
  }

  void SetUserOverride(int tier) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCpuPerformanceTierOverride, tier);
    base::RunLoop().RunUntilIdle();
  }

  void KillRendererProcessOfActiveTab(Browser* browser) {
    auto* process = browser->tab_strip_model()
                        ->GetActiveWebContents()
                        ->GetPrimaryMainFrame()
                        ->GetProcess();
    content::RenderProcessHostWatcher crash_observer(
        process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    process->Shutdown(-1);
    crash_observer.Wait();
  }

  int GetCpuPerformanceFromJs(Browser* browser) {
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    auto result = content::EvalJs(web_contents, "navigator.cpuPerformance");
    CHECK(result.is_int()) << "navigator.cpuPerformance should be an integer: "
                           << result;
    return result.ExtractInt();
  }

  GURL url() const {
    return embedded_test_server()->GetURL("localhost", "/title1.html");
  }

  int hardware_tier() const {
    CHECK_NE(-1, hardware_tier_);
    return hardware_tier_;
  }

  int policy_override_tier() const {
    CHECK_NE(-1, policy_override_tier_);
    return policy_override_tier_;
  }

  int user_override_tier() const {
    CHECK_NE(-1, user_override_tier_);
    return user_override_tier_;
  }

 private:
  int hardware_tier_ = -1;
  int policy_override_tier_ = -1;
  int user_override_tier_ = -1;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, NoPolicy) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, PolicyOverrideNormal) {
  SetPolicy(policy_override_tier());

  // Create a new normal browser.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(new_browser, url()));
  EXPECT_EQ(policy_override_tier(), GetCpuPerformanceFromJs(new_browser));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, PolicyOverrideIncognito) {
  SetPolicy(policy_override_tier());

  // Create a new incognito browser.
  Browser* incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito, url()));
  EXPECT_EQ(policy_override_tier(), GetCpuPerformanceFromJs(incognito));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, PolicyChangeNormalWindow) {
  // Step 1: Open first tab (no policy).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));

  // Step 2: Set a policy override.
  SetPolicy(policy_override_tier());

  // Old tab still has hardware tier.
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));

  // Step 3: Kill the renderer process, so that the old renderer is not reused.
  KillRendererProcessOfActiveTab(browser());

  // Step 4: Open new tab in second window, in a new normal browser.
  Browser* normal = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(normal, url()));

  // New tab has overridden tier.
  EXPECT_EQ(policy_override_tier(), GetCpuPerformanceFromJs(normal));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, PolicyChangeNormalTab) {
  // Step 1: Open first tab (no policy).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));

  // Step 2: Set a policy override.
  SetPolicy(policy_override_tier());

  // Old tab still has hardware tier.
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));

  // Step 3: Kill the renderer process, so that the old renderer is not reused.
  KillRendererProcessOfActiveTab(browser());

  // Step 4: Open second tab in the same window.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  // New tab has overridden tier.
  EXPECT_EQ(policy_override_tier(), GetCpuPerformanceFromJs(browser()));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, PolicyChangeIncognito) {
  // Step 1: Open first window (no policy).
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url()));
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));

  // Step 2: Set a policy override.
  SetPolicy(policy_override_tier());

  // Step 3: Open second window in a new incognito browser.
  Browser* incognito2 = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(incognito2, url()));

  // Old window still has hardware tier, new window has overridden tier.
  EXPECT_EQ(hardware_tier(), GetCpuPerformanceFromJs(browser()));
  EXPECT_EQ(policy_override_tier(), GetCpuPerformanceFromJs(incognito2));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, UserOverrideNormal) {
  // Set a user override.
  SetUserOverride(user_override_tier());

  // Create a new normal browser.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(new_browser, url()));
  EXPECT_EQ(user_override_tier(), GetCpuPerformanceFromJs(new_browser));
}

IN_PROC_BROWSER_TEST_F(CpuPerformancePolicyTest, PolicyWinsOverUserOverride) {
  // Set a user override.
  SetUserOverride(user_override_tier());

  // Set a different policy override.
  SetPolicy(policy_override_tier());

  // Create a new normal browser. Policy should win.
  Browser* new_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(new_browser, url()));
  EXPECT_EQ(policy_override_tier(), GetCpuPerformanceFromJs(new_browser));
}

}  // namespace policy
