// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from: //chrome/browser/policy/test/jit_policy_browsertest.cc

#include <vector>

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

enum DefaultV8OptimizerPolicyVariants {
  DISABLED_BY_DEFAULT,
  ENABLED_BY_DEFAULT,
  NOT_SET
};

}  // namespace

class V8OptimizerPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<DefaultV8OptimizerPolicyVariants> {
 public:
  V8OptimizerPolicyTest() = default;
  ~V8OptimizerPolicyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetPolicyValue(PolicyMap* map, const char* key, const char* value) {
    base::Value::List value_list;
    if (value) {
      value_list.Append(value);
    }
    SetPolicy(map, key, base::Value(std::move(value_list)));
  }

  // Set the allowed and blocked policies. Each of |allowed_site| and
  // |blocked_site| is a single domain ("foo.com") or domain wildcard
  // ("[*.]foo.com").
  void ConfigurePolicy(const char* allowed_site, const char* blocked_site) {
    PolicyMap policies;
    AddDefaultPolicy(&policies);

    SetPolicyValue(&policies, key::kJavaScriptOptimizerAllowedForSites,
                   allowed_site);
    SetPolicyValue(&policies, key::kJavaScriptOptimizerBlockedForSites,
                   blocked_site);

    provider_.UpdateChromePolicy(policies);
  }

  void NavigateAndExpectPolicyResult(const char* hostname,
                                     bool expect_disabled) {
    auto* render_frame_host = ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(hostname, "/title1.html"));
    EXPECT_EQ(expect_disabled,
              render_frame_host->GetProcess()->AreV8OptimizationsDisabled());
  }

 protected:
  void AddDefaultPolicy(PolicyMap* policies) {
    switch (GetParam()) {
      case DISABLED_BY_DEFAULT:
        SetPolicy(policies, key::kDefaultJavaScriptOptimizerSetting,
                  base::Value(CONTENT_SETTING_BLOCK));
        break;
      case ENABLED_BY_DEFAULT:
        SetPolicy(policies, key::kDefaultJavaScriptOptimizerSetting,
                  base::Value(CONTENT_SETTING_ALLOW));
        break;
      case NOT_SET:
        break;
    }
  }

  bool DetermineExpectedResultForDefault() {
    switch (GetParam()) {
      case DISABLED_BY_DEFAULT:
        return true;
      case ENABLED_BY_DEFAULT:
      case NOT_SET:
        return false;
    }
  }
};

IN_PROC_BROWSER_TEST_P(V8OptimizerPolicyTest, V8OptimizerAllowedAndDisallowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ConfigurePolicy("optimizer-enabled.com", "optimizer-disabled.com");

  GURL disabled_url =
      embedded_test_server()->GetURL("optimizer-disabled.com", "/title1.html");
  auto* render_frame_host =
      ui_test_utils::NavigateToURL(browser(), disabled_url);
  EXPECT_FALSE(render_frame_host->GetProcess()->IsJitDisabled());
  EXPECT_TRUE(render_frame_host->GetProcess()->AreV8OptimizationsDisabled());

  GURL enabled_url =
      embedded_test_server()->GetURL("optimizer-enabled.com", "/title1.html");
  render_frame_host = ui_test_utils::NavigateToURL(browser(), enabled_url);
  EXPECT_FALSE(render_frame_host->GetProcess()->IsJitDisabled());
  EXPECT_FALSE(render_frame_host->GetProcess()->AreV8OptimizationsDisabled());

  GURL default_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  render_frame_host = ui_test_utils::NavigateToURL(browser(), default_url);

  EXPECT_FALSE(render_frame_host->GetProcess()->IsJitDisabled());
  EXPECT_EQ(DetermineExpectedResultForDefault(),
            render_frame_host->GetProcess()->AreV8OptimizationsDisabled());
}

IN_PROC_BROWSER_TEST_P(V8OptimizerPolicyTest, V8OptimizerHostnameMatching) {
  // For brevity, this test only tests Deny rules, because Allow rules are
  // tested above.
  ASSERT_TRUE(embedded_test_server()->Start());

  // Check subdomains work.
  ConfigurePolicy(nullptr, "foo.com");
  NavigateAndExpectPolicyResult("foo.com", true);
  NavigateAndExpectPolicyResult("subdomain.foo.com", true);
  ConfigurePolicy(nullptr, "[*.]foo.com");
  NavigateAndExpectPolicyResult("subdomain.foo.com", true);

  const bool expected = DetermineExpectedResultForDefault();

  // Policy applies to different domain.
  ConfigurePolicy(nullptr, "foo.com");
  NavigateAndExpectPolicyResult("bar.com", expected);

  // Here there is an invalid policy as the V8 optimizer policies only support
  // eTLD+1 as origin.
  ConfigurePolicy(nullptr, "subdomain.foo.com");
  NavigateAndExpectPolicyResult("foo.com", expected);
  NavigateAndExpectPolicyResult("subdomain.foo.com", expected);
}

INSTANTIATE_TEST_SUITE_P(DefaultDisabled,
                         V8OptimizerPolicyTest,
                         testing::Values(DISABLED_BY_DEFAULT));
INSTANTIATE_TEST_SUITE_P(DefaultEnabled,
                         V8OptimizerPolicyTest,
                         testing::Values(ENABLED_BY_DEFAULT));
INSTANTIATE_TEST_SUITE_P(DefaultNotSet,
                         V8OptimizerPolicyTest,
                         testing::Values(NOT_SET));

}  // namespace policy
