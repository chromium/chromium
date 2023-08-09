// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace policy {

class BlockTruncatedCookiesPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  BlockTruncatedCookiesPolicyTest() {
    // Ensure that the feature is always enabled so that the behavior is only
    // controlled by the enterprise policy.
    feature_list_.InitAndEnableFeature(net::features::kBlockTruncatedCookies);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    UpdateBlockTruncatedCookiesPolicy(is_enabled());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void UpdateBlockTruncatedCookiesPolicy(bool enable) {
    PolicyMap policies;
    policies.Set(key::kBlockTruncatedCookies, POLICY_LEVEL_RECOMMENDED,
                 POLICY_SCOPE_USER, POLICY_SOURCE_COMMAND_LINE,
                 base::Value(enable), nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  bool is_enabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(BlockTruncatedCookiesPolicyTest, RunTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/empty.html")));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string cookie_js = "document.cookie = 'foo=ba\\x00r'; document.cookie;";

  std::string cookie_string = EvalJs(contents, cookie_js).ExtractString();

  if (is_enabled()) {
    EXPECT_EQ("", cookie_string);
  } else {
    EXPECT_EQ("foo=ba", cookie_string);
  }

  // Now change the policy and verify that it takes affect. The change should
  // cause the ProfileNetworkContextService pref change handler to fire, so
  // use `RunUntilIdle()` to hopefully ensure that that happens. That will
  // queue a message to Profile's NetworkContext's CookieManager that will
  // update its CookieSettings. To ensure that actually gets sent we'll flush
  // the corresponding message pipe. That CookieSettings should be shared by all
  // the per-renderer RestrictedCookieManagers, but just in case there's some
  // delay in that value propagating we will navigate to a new origin, which
  // should create a new RestrictedCookieManager and should reliably use the
  // updated setting. Using a new domain also means that we don't have to worry
  // about deleting any cookies that were set above.
  UpdateBlockTruncatedCookiesPolicy(!is_enabled());
  base::RunLoop().RunUntilIdle();
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("b.com", "/empty.html")));

  contents = browser()->tab_strip_model()->GetActiveWebContents();

  cookie_string = EvalJs(contents, cookie_js).ExtractString();

  // Check for the opposite behavior from above.
  if (is_enabled()) {
    EXPECT_EQ("foo=ba", cookie_string);
  } else {
    EXPECT_EQ("", cookie_string);
  }
}

INSTANTIATE_TEST_SUITE_P(BlockTruncatedCookiesPolicyTestP,
                         BlockTruncatedCookiesPolicyTest,
                         testing::Values(true, false));
}  // namespace policy
