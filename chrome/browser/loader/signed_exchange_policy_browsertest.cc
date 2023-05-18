// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/signed_exchange_browser_test_helper.h"
#include "ui/base/l10n/l10n_util.h"

class SignedExchangePolicyBrowserTest : public CertVerifierBrowserTest {
 public:
  SignedExchangePolicyBrowserTest() = default;

  SignedExchangePolicyBrowserTest(const SignedExchangePolicyBrowserTest&) =
      delete;
  SignedExchangePolicyBrowserTest& operator=(
      const SignedExchangePolicyBrowserTest&) = delete;

  ~SignedExchangePolicyBrowserTest() override = default;

 protected:
  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    sxg_test_helper_.InstallMockCert(mock_cert_verifier());
    sxg_test_helper_.InstallMockCertChainInterceptor();
  }

  void SetUpInProcessBrowserTestFixture() override {
    CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

 private:
  void SetUp() override {
    sxg_test_helper_.SetUp();
    InProcessBrowserTest::SetUp();
  }

  void TearDownOnMainThread() override {
    sxg_test_helper_.TearDownOnMainThread();
  }

  content::SignedExchangeBrowserTestHelper sxg_test_helper_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangePolicyBrowserTest, BlockList) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL inner_url("https://test.example.org/test/");
  const GURL url =
      embedded_test_server()->GetURL("/sxg/test.example.org_test.sxg");

  std::u16string expected_title(base::UTF8ToUTF16(inner_url.spec()));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(contents, expected_title);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  base::Value::List blocklist;
  blocklist.Append("test.example.org");
  policy::PolicyMap policies;
  policies.Set(policy::key::kURLBlocklist, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(blocklist)), nullptr);

#if BUILDFLAG(IS_CHROMEOS)
  policy::SetEnterpriseUsersProfileDefaults(&policies);
#endif
  policy_provider_.UpdateChromePolicy(policies);
  base::RunLoop loop;
  loop.RunUntilIdle();

  // Updates of the URLBlocklist are done on IO, after building the blocklist
  // on the blocking pool, which is initiated from IO.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  content::RunAllTasksUntilIdle();
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  std::u16string blocked_page_title(u"test.example.org");
  EXPECT_EQ(blocked_page_title, contents->GetTitle());

  // Verify that the expected error page is being displayed.
  EXPECT_EQ(
      true,
      content::EvalJs(
          contents, content::JsReplace(
                        "var textContent = document.body.textContent;"
                        "var hasError = "
                        "textContent.indexOf($1) >= 0;"
                        "hasError;",
                        l10n_util::GetStringUTF8(
                            IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR))));
}
