// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"

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

class BackForwardCacheURLBlocklistPolicyBrowserTest : public PolicyTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures({features::kBackForwardCache}, {});
    PolicyTest::SetUpCommandLine(command_line);
  }

  void SetUrlPolicies(const std::vector<std::string>& blocklist,
                      const std::vector<std::string>& allowlist = {}) {
    base::ListValue blocklist_value;
    for (const std::string& pattern : blocklist) {
      blocklist_value.Append(pattern);
    }
    base::ListValue allowlist_value;
    for (const std::string& pattern : allowlist) {
      allowlist_value.Append(pattern);
    }
    PolicyMap policies;
    SetPolicy(&policies, key::kURLBlocklist,
              base::Value(std::move(blocklist_value)));
    SetPolicy(&policies, key::kURLAllowlist,
              base::Value(std::move(allowlist_value)));
    UpdateProviderPolicy(policies);
    FlushBlocklistPolicy();
  }

  void BlockUrl(const std::string& pattern) { SetUrlPolicies({pattern}); }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  content::RenderFrameHost* current_render_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
  }

  void ExpectBlockedByAdministrator(
      const content::TestNavigationObserver& observer) {
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR,
              observer.last_net_error_code());
    EXPECT_EQ(
        content::PAGE_TYPE_ERROR,
        web_contents()->GetController().GetLastCommittedEntry()->GetPageType());
  }

  void ExpectNotRestoredDueToDomainNotAllowed() {
    histogram_tester_.ExpectUniqueSample(
        "BackForwardCache.HistoryNavigationOutcome.NotRestoredReason",
        content::BackForwardCache::NotRestoredReason::kDomainNotAllowed, 1);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

// Tests that a page blocked by URLBlocklist is not stored in BackForwardCache
// when navigating away from it.
IN_PROC_BROWSER_TEST_F(BackForwardCacheURLBlocklistPolicyBrowserTest,
                       BlocklistedPageCannotEnterBackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to url_a while allowed, then block it.
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());
  BlockUrl("a.com");

  // 2) Navigate to url_b. rfh_a must not enter BFCache because a.com is
  // blocked.
  ASSERT_TRUE(NavigateToUrl(url_b, this));
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Navigate back to url_a: it must not be restored from BFCache and must
  // fail with ERR_BLOCKED_BY_ADMINISTRATOR instead.
  content::TestNavigationObserver back_observer(web_contents());
  web_contents()->GetController().GoBack();
  back_observer.Wait();
  ExpectBlockedByAdministrator(back_observer);
  ExpectNotRestoredDueToDomainNotAllowed();
}

// Tests that reloading a page blocked by URLBlocklist keeps showing the error
// page instead of alternating with the cached page.
IN_PROC_BROWSER_TEST_F(BackForwardCacheURLBlocklistPolicyBrowserTest,
                       ReloadBlocklistedPageDoesNotAlternate) {
  // The page sends a COOP header and the error page doesn't, so the error page
  // commits in a new BrowsingInstance. Without that swap the old page would
  // never be eligible for BFCache. COOP only applies on secure origins.
  // TODO(crbug.com/567169242): Reloads shouldn't store/restore BFCache entries
  // at all; this test exercises that pre-existing behavior.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers();
  ASSERT_TRUE(https_server.Start());

  GURL url_a(https_server.GetURL(
      "a.test", "/set-header?Cross-Origin-Opener-Policy: same-origin"));

  // 1) Navigate to url_a while allowed, then block it.
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());
  BlockUrl("a.test");

  // 2) Reload #1: the policy throttle blocks the reload and commits an error
  // page. rfh_a must be deleted rather than stored in BFCache.
  content::TestNavigationObserver reload1_observer(web_contents());
  web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
  reload1_observer.Wait();
  ExpectBlockedByAdministrator(reload1_observer);
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 3) Reload #2: must still show the error page rather than restoring rfh_a
  // from BFCache.
  content::TestNavigationObserver reload2_observer(web_contents());
  web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                         /*check_for_repost=*/false);
  reload2_observer.Wait();
  ExpectBlockedByAdministrator(reload2_observer);
}

// Tests that a page already in BackForwardCache is evicted on Back navigation
// if its URL was added to URLBlocklist after it was cached.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheURLBlocklistPolicyBrowserTest,
    PageInBackForwardCacheEvictedWhenBlocklistedBeforeRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to url_a, then to url_b while allowed. url_a must enter
  // BFCache.
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());
  ASSERT_TRUE(NavigateToUrl(url_b, this));
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 2) Block a.com while url_a is already in BFCache.
  BlockUrl("a.com");

  // 3) Navigate back to url_a: the cached page must be evicted and the
  // navigation must fall back to the network, where it is blocked.
  content::TestNavigationObserver back_observer(web_contents());
  web_contents()->GetController().GoBack();
  back_observer.Wait();
  ExpectBlockedByAdministrator(back_observer);
  ExpectNotRestoredDueToDomainNotAllowed();
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

// Tests that a page already in BackForwardCache is evicted on Back navigation
// if one of its subframes' URL was added to URLBlocklist after it was cached,
// even though the main frame URL is still allowed.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheURLBlocklistPolicyBrowserTest,
    PageInBackForwardCacheEvictedWhenSubframeBlocklistedBeforeRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  GURL url_sub(embedded_test_server()->GetURL("sub.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to url_a and navigate its subframe to url_sub.
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(), "test", url_sub));

  // 2) Navigate to url_b while allowed. url_a must enter BFCache.
  ASSERT_TRUE(NavigateToUrl(url_b, this));
  ASSERT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Block only the subframe's host while url_a is in BFCache.
  BlockUrl("sub.com");

  // 4) Navigate back to url_a: the cached page must be evicted because of its
  // subframe, and the (still allowed) main frame must be loaded again.
  content::TestNavigationObserver back_observer(web_contents());
  web_contents()->GetController().GoBack();
  back_observer.Wait();
  EXPECT_EQ(url_a, current_render_frame_host()->GetLastCommittedURL());
  EXPECT_EQ(
      content::PAGE_TYPE_NORMAL,
      web_contents()->GetController().GetLastCommittedEntry()->GetPageType());
  ExpectNotRestoredDueToDomainNotAllowed();
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

// Tests that subframes the URLBlocklist navigation throttle never checks
// (about:blank, about:srcdoc, blob:) don't prevent an allowed page from
// entering BackForwardCache when URLBlocklist = ["*"].
IN_PROC_BROWSER_TEST_F(BackForwardCacheURLBlocklistPolicyBrowserTest,
                       NonNetworkSubframesDoNotPreventBackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUrlPolicies({"*"}, {"a.com", "b.com"});

  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));
  constexpr auto kInBackForwardCache =
      content::RenderFrameHost::LifecycleState::kInBackForwardCache;

  for (const char* path : {"/iframe_about_blank.html", "/iframe_srcdoc.html",
                           "/iframe_blob.html"}) {
    SCOPED_TRACE(path);
    GURL url_a(embedded_test_server()->GetURL("a.com", path));

    // 1) Navigate to url_a, whose only subframe has a non-network URL.
    ASSERT_TRUE(NavigateToUrl(url_a, this));
    ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
    content::RenderFrameHostWrapper rfh_a(current_render_frame_host());
    content::RenderFrameHost* subframe = content::ChildFrameAt(rfh_a.get(), 0);
    ASSERT_TRUE(subframe);
    EXPECT_FALSE(subframe->GetLastCommittedURL().SchemeIsHTTPOrHTTPS());

    // 2) Navigate to url_b. url_a must enter BFCache.
    ASSERT_TRUE(NavigateToUrl(url_b, this));
    EXPECT_TRUE(!rfh_a.IsDestroyed() &&
                rfh_a->IsInLifecycleState(kInBackForwardCache));
  }
}

// Tests that if a subframe is already blocked by URLBlocklist at load time and
// commits an error document, the allowed main frame can still enter and be
// restored from BackForwardCache.
IN_PROC_BROWSER_TEST_F(
    BackForwardCacheURLBlocklistPolicyBrowserTest,
    BlocklistedSubframeErrorDocumentDoesNotPreventBackForwardCache) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url_a(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  GURL url_sub(embedded_test_server()->GetURL("sub.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Block sub.com before loading it in url_a's subframe.
  BlockUrl("sub.com");
  ASSERT_TRUE(NavigateToUrl(url_a, this));
  content::RenderFrameHostWrapper rfh_a(current_render_frame_host());
  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(), "test", url_sub));
  content::RenderFrameHost* subframe = content::ChildFrameAt(rfh_a.get(), 0);
  ASSERT_TRUE(subframe);
  EXPECT_TRUE(subframe->IsErrorDocument());
  EXPECT_EQ(url_sub, subframe->GetLastCommittedURL());

  // 2) Navigate to url_b. url_a must still enter BFCache.
  ASSERT_TRUE(NavigateToUrl(url_b, this));
  ASSERT_TRUE(
      !rfh_a.IsDestroyed() &&
      rfh_a->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kInBackForwardCache));

  // 3) Navigate back to url_a: rfh_a must be restored from BFCache with its
  // error-document subframe intact.
  content::TestNavigationObserver back_observer(web_contents());
  web_contents()->GetController().GoBack();
  back_observer.Wait();
  EXPECT_EQ(rfh_a.get(), current_render_frame_host());
  subframe = content::ChildFrameAt(rfh_a.get(), 0);
  ASSERT_TRUE(subframe);
  EXPECT_TRUE(subframe->IsErrorDocument());
}

}  // namespace policy
