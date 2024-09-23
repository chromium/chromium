// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

class Browser;

namespace policy {

enum class CacheControlNoStorePagePolicy {
  kDefault,
  kAllowed,
  kDisallowed,
};
struct BackForwardCacheWithCacheControlNoStorePagePolicyTestParam {
  CacheControlNoStorePagePolicy policy_value;
  bool expected_allow_bfcache_ccns_page;
};

class BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest
    : public PolicyTest,
      public ::testing::WithParamInterface<
          BackForwardCacheWithCacheControlNoStorePagePolicyTestParam> {
 public:
  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    switch (info.param.policy_value) {
      case CacheControlNoStorePagePolicy::kDefault:
        return "Default";
      case CacheControlNoStorePagePolicy::kAllowed:
        return "Allowed";
      case CacheControlNoStorePagePolicy::kDisallowed:
        return "Disallowed";
    }
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache, {}},
         {features::kCacheControlNoStoreEnterBackForwardCache,
          {{"level", "restore-unless-http-only-cookie-change"}}}},
        {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    if (GetParam().policy_value == CacheControlNoStorePagePolicy::kDefault) {
      return;
    }

    // Set up the policy value for
    // `kAllowBackForwardCacheForCacheControlNoStorePageEnabled`.
    PolicyMap policies;
    SetPolicy(&policies,
              key::kAllowBackForwardCacheForCacheControlNoStorePageEnabled,
              base::Value(GetParam().policy_value ==
                          CacheControlNoStorePagePolicy::kAllowed));
    provider_.UpdateChromePolicy(policies);
  }

  content::RenderFrameHost* current_render_frame_host() {
    return chrome_test_utils::GetActiveWebContents(this)->GetPrimaryMainFrame();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

const auto test_suite_value = ::testing::Values(
    BackForwardCacheWithCacheControlNoStorePagePolicyTestParam{
        CacheControlNoStorePagePolicy::kDefault,
        /* expected_allow_bfcache_ccns_page= */ true},
    BackForwardCacheWithCacheControlNoStorePagePolicyTestParam{
        CacheControlNoStorePagePolicy::kAllowed,
        /* expected_allow_bfcache_ccns_page= */ true},
    BackForwardCacheWithCacheControlNoStorePagePolicyTestParam{
        CacheControlNoStorePagePolicy::kDisallowed,
        /* expected_allow_bfcache_ccns_page= */ false});

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest,
    test_suite_value,
    &BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest::
        DescribeParams);

// Test that a page loaded with "Cache-Control:no-store" header cannot enter
// BackForwardCache if the ContentBrowserClient disables BFCache for CCNS pages.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest,
    PolicyIsFollowed) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());

  // 2) Navigate away. If the enterprise policy disallows BFCaching CCNS page,
  // `rfh_a` should not enter BFCache. Otherwise, `rfh_a` should be stored in
  // BFCache.
  ASSERT_TRUE(NavigateToUrl(url_b, this));
  content::RenderFrameHostWrapper rfh_b(current_render_frame_host());
  if (GetParam().expected_allow_bfcache_ccns_page) {
    ASSERT_TRUE(rfh_a->GetLifecycleState() ==
                content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  } else {
    ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  }

  // 3) Verify that the page without CCNS is eligible for BFCache.
  ASSERT_TRUE(NavigateToUrl(url_c, this));
  ASSERT_TRUE(rfh_b->GetLifecycleState() ==
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test that the `ShouldAllowBackForwardCacheForCacheControlNoStorePage()`
// returns the correct value for different policy settings.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest,
    ShouldAllowBackForwardCacheForCacheControlNoStorePage) {
  bool should_allow_bfcache_ccns_page =
      content::GetContentClientForTesting()
          ->browser()
          ->ShouldAllowBackForwardCacheForCacheControlNoStorePage(
              current_render_frame_host()->GetBrowserContext());

  ASSERT_EQ(should_allow_bfcache_ccns_page,
            GetParam().expected_allow_bfcache_ccns_page);
}

class BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTestKioskMode
    : public BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kKioskMode);
    BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest::
        SetUpCommandLine(command_line);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTestKioskMode,
    test_suite_value,
    &BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTest::
        DescribeParams);

// Test that a page loaded with "Cache-Control:no-store" header cannot enter
// BackForwardCache if the ContentBrowserClient disables BFCache for CCNS pages.
IN_PROC_BROWSER_TEST_P(
    BackForwardCacheWithCacheControlNoStorePagePolicyBrowserTestKioskMode,
    PolicyIsOverridenByKioskMode) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL(
      "a.com", "/set-header?Cache-Control: no-store"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title1.html"));

  // 1) Load the document and specify no-store for the main resource.
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());

  // 2) Navigate away. If the enterprise policy disallows BFCaching CCNS page,
  // `rfh_a` should not enter BFCache. Otherwise, `rfh_a` should be stored in
  // BFCache.
  ASSERT_TRUE(NavigateToUrl(url_b, this));
  content::RenderFrameHostWrapper rfh_b(current_render_frame_host());
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Verify that the page without CCNS is eligible for BFCache.
  ASSERT_TRUE(NavigateToUrl(url_c, this));
  ASSERT_TRUE(rfh_b->GetLifecycleState() ==
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

}  // namespace policy
