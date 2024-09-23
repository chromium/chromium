// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

enum DefaultJitPolicyVariants {
  DISABLED_BY_DEFAULT,
  ENABLED_BY_DEFAULT,
  NOT_SET
};

}  // namespace

class JITPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<DefaultJitPolicyVariants> {
 public:
  JITPolicyTest() = default;
  ~JITPolicyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    // This is needed for this test to run properly on platforms where
    //  --site-per-process isn't the default, such as Android.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;

    AddDefaultPolicy(&policies);

    base::Value::List block_list;
    block_list.Append("jit-disabled.com");
    SetPolicy(&policies, key::kJavaScriptJitBlockedForSites,
              base::Value(std::move(block_list)));

    base::Value::List allow_list;
    allow_list.Append("jit-enabled.com");
    SetPolicy(&policies, key::kJavaScriptJitAllowedForSites,
              base::Value(std::move(allow_list)));

    provider_.UpdateChromePolicy(policies);
  }

 protected:
  void AddDefaultPolicy(PolicyMap* policies);
  void ExpectThatPolicyDisablesJitOnUrl(const char* policy_value,
                                        const char* url_value,
                                        bool expect_jit_disabled);
  bool DetermineExpectedResultForDefault();
};

void JITPolicyTest::AddDefaultPolicy(PolicyMap* policies) {
  switch (GetParam()) {
    case DISABLED_BY_DEFAULT:
      SetPolicy(policies, key::kDefaultJavaScriptJitSetting,
                base::Value(CONTENT_SETTING_BLOCK));
      break;
    case ENABLED_BY_DEFAULT:
      SetPolicy(policies, key::kDefaultJavaScriptJitSetting,
                base::Value(CONTENT_SETTING_ALLOW));
      break;
    case NOT_SET:
      break;
  }
}

bool JITPolicyTest::DetermineExpectedResultForDefault() {
  switch (GetParam()) {
    case DISABLED_BY_DEFAULT:
      return true;
    case ENABLED_BY_DEFAULT:
    case NOT_SET:
      return false;
  }
}

void JITPolicyTest::ExpectThatPolicyDisablesJitOnUrl(const char* policy_value,
                                                     const char* url_value,
                                                     bool expect_jit_disabled) {
  // This clears and resets the policies set-up in
  // SetUpInProcessBrowserTestFixture.
  PolicyMap policies;
  AddDefaultPolicy(&policies);

  base::Value::List block_list;
  block_list.Append(policy_value);
  SetPolicy(&policies, key::kJavaScriptJitBlockedForSites,
            base::Value(std::move(block_list)));

  UpdateProviderPolicy(policies);

  GURL blocked_url = embedded_test_server()->GetURL(url_value, "/title1.html");
  auto* render_frame_host =
      ui_test_utils::NavigateToURL(browser(), blocked_url);
  EXPECT_EQ(expect_jit_disabled,
            render_frame_host->GetProcess()->IsJitDisabled());
}

IN_PROC_BROWSER_TEST_P(JITPolicyTest, JitPolicyTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL disabled_url =
      embedded_test_server()->GetURL("jit-disabled.com", "/title1.html");
  auto* render_frame_host =
      ui_test_utils::NavigateToURL(browser(), disabled_url);
  EXPECT_TRUE(render_frame_host->GetProcess()->IsJitDisabled());

  GURL enabled_url =
      embedded_test_server()->GetURL("jit-enabled.com", "/title1.html");
  render_frame_host = ui_test_utils::NavigateToURL(browser(), enabled_url);
  EXPECT_FALSE(render_frame_host->GetProcess()->IsJitDisabled());

  GURL default_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  render_frame_host = ui_test_utils::NavigateToURL(browser(), default_url);

  EXPECT_EQ(DetermineExpectedResultForDefault(),
            render_frame_host->GetProcess()->IsJitDisabled());
}

IN_PROC_BROWSER_TEST_P(JITPolicyTest, JitDomainTest) {
  // For brevity, this test only tests Deny rules, because Allow rules are
  // tested above.
  ASSERT_TRUE(embedded_test_server()->Start());
  // Check subdomains work.
  ExpectThatPolicyDisablesJitOnUrl("foo.com", "foo.com",
                                   /*expect_jit_disabled=*/true);
  ExpectThatPolicyDisablesJitOnUrl("foo.com", "subdomain.foo.com",
                                   /*expect_jit_disabled=*/true);
  ExpectThatPolicyDisablesJitOnUrl("[*.]foo.com", "subdomain.foo.com",
                                   /*expect_jit_disabled=*/true);

  bool expected_result_for_default = DetermineExpectedResultForDefault();

  // Policy applies to different domain.
  ExpectThatPolicyDisablesJitOnUrl(
      "foo.com", "bar.com",
      /*expect_jit_disabled=*/expected_result_for_default);

  // Here there is an invalid policy as the JavaScript JIT policies only support
  // eTLD+1 as origin.
  ExpectThatPolicyDisablesJitOnUrl(
      "subdomain.foo.com", "foo.com",
      /*expect_jit_disabled=*/expected_result_for_default);
  ExpectThatPolicyDisablesJitOnUrl(
      "subdomain.foo.com", "subdomain.foo.com",
      /*expect_jit_disabled=*/expected_result_for_default);
}

INSTANTIATE_TEST_SUITE_P(DefaultDisabled,
                         JITPolicyTest,
                         testing::Values(DISABLED_BY_DEFAULT));
INSTANTIATE_TEST_SUITE_P(DefaultEnabled,
                         JITPolicyTest,
                         testing::Values(ENABLED_BY_DEFAULT));
INSTANTIATE_TEST_SUITE_P(DefaultNotSet,
                         JITPolicyTest,
                         testing::Values(NOT_SET));

}  // namespace policy
