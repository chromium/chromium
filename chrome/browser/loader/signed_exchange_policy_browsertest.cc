// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"

class SignedExchangePolicyBrowserTest : public CertVerifierBrowserTest {
 public:
  SignedExchangePolicyBrowserTest() = default;
  ~SignedExchangePolicyBrowserTest() override = default;

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    sxg_test_helper_.InstallMockCert(mock_cert_verifier());
    sxg_test_helper_.InstallMockCertChainInterceptor();
  }

  void SetUpInProcessBrowserTestFixture() override {
    CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture();
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  policy::MockConfigurationPolicyProvider policy_provider_;

 private:
  void SetUp() override {
    sxg_test_helper_.SetUp();
    InProcessBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
  }

  content::SignedExchangeBrowserTestHelper sxg_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangePolicyBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SignedExchangePolicyBrowserTest, BlackList) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL inner_url("https://test.example.org/test/");
  const GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  base::string16 expected_title(base::UTF8ToUTF16(inner_url.spec()));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(contents, expected_title);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  base::ListValue blacklist;
  blacklist.AppendString("test.example.org");
  policy::PolicyMap policies;
  policies.Set(policy::key::kURLBlacklist, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               blacklist.CreateDeepCopy(), nullptr);

#if defined(OS_CHROMEOS)
  policy::SetEnterpriseUsersDefaults(&policies);
#endif
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop loop;
  loop.RunUntilIdle();

  // Updates of the URLBlacklist are done on IO, after building the blacklist
  // on the blocking pool, which is initiated from IO.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

  ui_test_utils::NavigateToURL(browser(), url);

  base::string16 blocked_page_title(base::UTF8ToUTF16("test.example.org"));
  EXPECT_EQ(blocked_page_title, contents->GetTitle());

  // Verify that the expected error page is being displayed.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "var textContent = document.body.textContent;"
      "var hasError = textContent.indexOf('ERR_BLOCKED_BY_ADMINISTRATOR') >= 0;"
      "domAutomationController.send(hasError);",
      &result));
  EXPECT_TRUE(result);
}
