// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/security_state/core/features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"

namespace {
// SecureOriginWhitelistBrowsertests differ in the setup of the browser. Since
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

// End-to-end browser test that ensures the secure origin whitelist works when
// supplied via command-line or policy.
// SecureOriginWhitelistUnittest will test the list parsing.
class SecureOriginWhitelistBrowsertest
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
    EXPECT_CALL(provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

    base::Value::ListStorage urls;
    if (variant == TestVariant::kPolicy || variant == TestVariant::kPolicyOld ||
        variant == TestVariant::kPolicyOldAndNew) {
      urls.push_back(base::Value(BaseURL()));
    } else if (variant == TestVariant::kPolicy2) {
      urls.push_back(base::Value(BaseURL()));
      urls.push_back(base::Value(OtherURL()));
    } else if (variant == TestVariant::kPolicy3) {
      urls.push_back(base::Value(OtherURL()));
      urls.push_back(base::Value(BaseURL()));
    }

    policy::PolicyMap values;
    values.Set((variant == TestVariant::kPolicyOld ||
                variant == TestVariant::kPolicyOldAndNew)
                   ? policy::key::kUnsafelyTreatInsecureOriginAsSecure
                   : policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(std::move(urls)), nullptr);
    if (variant == TestVariant::kPolicyOldAndNew) {
      base::Value::ListStorage other_urls;
      other_urls.push_back(base::Value(OtherURL()));
      values.Set(policy::key::kOverrideSecurityRestrictionsOnInsecureOrigin,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 std::make_unique<base::Value>(std::move(other_urls)), nullptr);
    }

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
  policy::MockConfigurationPolicyProvider provider_;
};

INSTANTIATE_TEST_SUITE_P(SecureOriginWhitelistBrowsertest,
                         SecureOriginWhitelistBrowsertest,
                         testing::Values(TestVariant::kNone,
                                         TestVariant::kCommandline,
// The legacy policy isn't defined on ChromeOS or Android, so skip tests that
// use it on those platforms.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
                                         TestVariant::kPolicyOld,
                                         TestVariant::kPolicyOldAndNew,
#endif
                                         TestVariant::kPolicy,
                                         TestVariant::kPolicy2,
                                         TestVariant::kPolicy3));

IN_PROC_BROWSER_TEST_P(SecureOriginWhitelistBrowsertest, Simple) {
  GURL url = embedded_test_server()->GetURL(
      "example.com", "/secure_origin_whitelist_browsertest.html");
  ui_test_utils::NavigateToURL(browser(), url);

  base::string16 secure(base::ASCIIToUTF16("secure context"));
  base::string16 insecure(base::ASCIIToUTF16("insecure context"));

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
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "otherexample.com", "/secure_origin_whitelist_browsertest.html"));
    EXPECT_EQ(next_title_watcher.WaitAndGetTitle(), secure);
  } else {
    EXPECT_EQ(title_watcher.WaitAndGetTitle(),
              ExpectSecureContext() ? secure : insecure);
  }
}

class SecureOriginWhitelistBrowsertestWithMarkHttpDangerous
    : public SecureOriginWhitelistBrowsertest {
 public:
  SecureOriginWhitelistBrowsertestWithMarkHttpDangerous() {
    // TODO(crbug.com/917693): Remove this forced feature/param when the feature
    // fully launches.
    feature_list_.InitAndEnableFeatureWithParameters(
        security_state::features::kMarkHttpAsFeature,
        {{security_state::features::kMarkHttpAsFeatureParameterName,
          security_state::features::kMarkHttpAsParameterDangerous}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that whitelisted insecure origins are correctly set as security level
// NONE instead of the default level DANGEROUS.
IN_PROC_BROWSER_TEST_P(SecureOriginWhitelistBrowsertestWithMarkHttpDangerous,
                       SecurityIndicators) {
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "example.com", "/secure_origin_whitelist_browsertest.html"));
  auto* helper = SecurityStateTabHelper::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(helper);

  if (GetParam() == TestVariant::kPolicyOldAndNew) {
    // When both policies are set, the new policy overrides the old policy.
    EXPECT_EQ(security_state::DANGEROUS, helper->GetSecurityLevel());
    ui_test_utils::NavigateToURL(
        browser(),
        embedded_test_server()->GetURL(
            "otherexample.com", "/secure_origin_whitelist_browsertest.html"));
    EXPECT_EQ(security_state::NONE, helper->GetSecurityLevel());
  } else {
    EXPECT_EQ(ExpectSecureContext() ? security_state::NONE
                                    : security_state::DANGEROUS,
              helper->GetSecurityLevel());
  }
}
