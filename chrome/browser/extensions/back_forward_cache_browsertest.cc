// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

class ExtensionBackForwardCacheBrowserTest : public ExtensionBrowserTest {
 public:
  explicit ExtensionBackForwardCacheBrowserTest(
      bool allow_content_scripts = true) {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"content_injection_supported",
            allow_content_scripts ? "true" : "false"}}}},
        {features::kBackForwardCacheMemoryControls});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ExtensionBackForwardCacheContentScriptDisabledBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheContentScriptDisabledBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(/*allow_content_scripts=*/false) {}
};

IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheContentScriptDisabledBrowserTest,
    ScriptDisallowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHost* rfh_a =
      ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  content::RenderFrameHost* rfh_b =
      ui_test_utils::NavigateToURL(browser(), url_b);
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Expect that |rfh_a| is destroyed as it wouldn't be placed in the cache.
  EXPECT_TRUE(delete_observer_rfh_a.deleted());
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest, ScriptAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHost* rfh_a =
      ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  content::RenderFrameHost* rfh_b =
      ui_test_utils::NavigateToURL(browser(), url_b);
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Ensure that |rfh_a| is in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheContentScriptDisabledBrowserTest,
    CSSDisallowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHost* rfh_a =
      ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  content::RenderFrameHost* rfh_b =
      ui_test_utils::NavigateToURL(browser(), url_b);
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Expect that |rfh_a| is destroyed as it wouldn't be placed in the cache.
  EXPECT_TRUE(delete_observer_rfh_a.deleted());
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest, CSSAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHost* rfh_a =
      ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  content::RenderFrameHost* rfh_b =
      ui_test_utils::NavigateToURL(browser(), url_b);
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Ensure that |rfh_a| is in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_TRUE(rfh_a->IsInBackForwardCache());
}

}  // namespace extensions
