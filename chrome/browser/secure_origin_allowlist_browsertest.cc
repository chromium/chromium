// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"

namespace {
// SecureOriginAllowlistBrowsertests differ in the setup of the browser. Since
// the setup is done before the actual test is run, we need to parameterize our
// tests outside of the actual test bodies. We use test variants for this,
// instead of the usual setup of mulitple tests.
enum class TestVariant {
  kNone,
  kCommandline,
  kPolicyOld,
  kPolicy,
  kPolicy2,
  kPolicy3,
  kPolicyOldAndNew,
};
}  // namespace

// End-to-end browser test that ensures the secure origin allowlist works when
// supplied via command-line or policy.
class SecureOriginAllowlistBrowsertest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<TestVariant> {
 public:
  void SetUpOnMainThread() override {
    // We need this, so we can request the test page from 'http://foo.com'.
    // (Which, unlike 127.0.0.1, is considered an insecure origin.)
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // We need to know the server port to know what to add to the command-line.
    // The port number changes with every test run. Thus, we start the server
    // here. And since all tests, not just the variant with the command-line,
    // need the embedded server, we unconditionally start it here.
    EXPECT_TRUE(embedded_test_server()->Start());

    if (GetParam() != TestVariant::kCommandline)
      return;

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure, BaseURL());
  }

  void SetUpInProcessBrowserTestFixture() override {
    TestVariant variant = GetParam();
    if (variant != TestVariant::kPolicyOld && variant != TestVariant::kPolicy &&
        variant != TestVariant::kPolicy2 && variant != TestVariant::kPolicy3 &&
        variant != TestVariant::kPolicyOldAndNew)
      return;

    // We setup the policy here, because the policy must be 'live' before
    // the renderer is created, since the value for this policy is passed
    // to the renderer via a command-line. Setting the policy in the test
    // itself or in SetUpOnMainThread works for update-able policies, but
    // is too late for this one.
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    base::Value::List urls;
    if (variant == TestVariant::kPolicy || variant == TestVariant::kPolicyOld ||
        variant == TestVariant::kPolicyOldAndNew) {
      urls.Append(BaseURL());
    } else if (variant == TestVariant::kPolicy2) {
      urls.Append(BaseURL());
      urls.Append(OtherURL());
    } else if (variant == TestVariant::kPolicy3) {
      urls.Append(OtherURL());
      urls.Append(BaseURL());
    }

    policy::PolicyMap values;
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
    values.Set((variant == TestVariant::kPolicyOld ||
                variant == TestVariant::kPolicyOldAndNew)
                   ? policy::key::kUnsafelyTreatInsecureOriginAsSecure
                   : policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(urls)),
               nullptr);
    if (variant == TestVariant::kPolicyOldAndNew) {
      base::Value::List other_urls;
      other_urls.Append(OtherURL());
      values.Set(policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(other_urls)), nullptr);
    }
#else
    values.Set(policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD, base::Value(std::move(urls)),
               nullptr);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

    provider_.UpdateChromePolicy(values);
  }

  bool ExpectSecureContext() { return GetParam() != TestVariant::kNone; }

  std::string BaseURL() {
    return embedded_test_server()->GetURL("example.com", "/").spec();
  }

  std::string OtherURL() {
    return embedded_test_server()->GetURL("otherexample.com", "/").spec();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

INSTANTIATE_TEST_SUITE_P(SecureOriginAllowlistBrowsertest,
                         SecureOriginAllowlistBrowsertest,
                         testing::Values(TestVariant::kNone,
                                         TestVariant::kCommandline,
// The legacy policy isn't defined on ChromeOS or Android, so skip tests that
// use it on those platforms.
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
                                         TestVariant::kPolicyOld,
                                         TestVariant::kPolicyOldAndNew,
#endif
                                         TestVariant::kPolicy,
                                         TestVariant::kPolicy2,
                                         TestVariant::kPolicy3));

IN_PROC_BROWSER_TEST_P(SecureOriginAllowlistBrowsertest, Simple) {
  GURL url = embedded_test_server()->GetURL(
      "example.com", "/secure_origin_allowlist_browsertest.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::u16string secure(u"secure context");
  std::u16string insecure(u"insecure context");

  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), secure);
  title_watcher.AlsoWaitForTitle(insecure);

  if (GetParam() == TestVariant::kPolicyOldAndNew) {
    // When both policies are set, the new one should take precedence over the
    // old one.
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), insecure);
    content::TitleWatcher next_title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), secure);
    next_title_watcher.AlsoWaitForTitle(insecure);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "otherexample.com", "/secure_origin_allowlist_browsertest.html")));
    EXPECT_EQ(next_title_watcher.WaitAndGetTitle(), secure);
  } else {
    EXPECT_EQ(title_watcher.WaitAndGetTitle(),
              ExpectSecureContext() ? secure : insecure);
  }
}

IN_PROC_BROWSER_TEST_P(SecureOriginAllowlistBrowsertest, SecurityIndicators) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "example.com", "/secure_origin_allowlist_browsertest.html")));
  auto* helper = SecurityStateTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(helper);

  if (GetParam() == TestVariant::kPolicyOldAndNew) {
    // When both policies are set, the new policy overrides the old policy.
    EXPECT_EQ(security_state::WARNING, helper->GetSecurityLevel());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "otherexample.com", "/secure_origin_allowlist_browsertest.html")));
    EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
  } else {
    EXPECT_EQ(
        ExpectSecureContext() ? security_state::NONE : security_state::WARNING,
        helper->GetSecurityLevel());
  }
}
