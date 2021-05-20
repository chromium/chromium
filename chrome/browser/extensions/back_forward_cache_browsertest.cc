// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace extensions {

class ExtensionBackForwardCacheBrowserTest : public ExtensionBrowserTest {
 public:
  explicit ExtensionBackForwardCacheBrowserTest(
      bool all_extensions_allowed = true,
      bool allow_content_scripts = true) {
    // If `allow_content_scripts` is true then `all_extensions_allowed` must
    // also be true.
    DCHECK(!(allow_content_scripts && !all_extensions_allowed));
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"content_injection_supported",
            allow_content_scripts ? "true" : "false"},
           {"TimeToLiveInBackForwardCacheInSeconds", "3600"},
           {"all_extensions_allowed",
            all_extensions_allowed ? "true" : "false"}}}},
        {features::kBackForwardCacheMemoryControls});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void RunChromeRuntimeTest(const std::string& action) {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                          .AppendASCII("content_script"));
    ASSERT_TRUE(extension);

    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
    GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

    // 1) Navigate to A.
    content::RenderFrameHost* rfh_a =
        ui_test_utils::NavigateToURL(browser(), url_a);
    content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

    constexpr int kMessagingBucket =
        (static_cast<int>(content::BackForwardCache::DisabledSource::kEmbedder)
         << 16) +
        static_cast<int>(
            back_forward_cache::DisabledReasonId::kExtensionMessaging);

    EXPECT_TRUE(ExecJs(
        rfh_a, base::StringPrintf(action.c_str(), extension->id().c_str())));

    EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                     "BackForwardCache.HistoryNavigationOutcome."
                     "DisabledForRenderFrameHostReason2",
                     kMessagingBucket));
    // 2) Navigate to B.
    ui_test_utils::NavigateToURL(browser(), url_b);

    // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache
    // since it uses the chrome.runtime API.
    delete_observer_rfh_a.WaitUntilDeleted();

    // 3) Go back to A.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents));

    // Validate that the not restored reason is `ExtensionMessaging` due to the
    // chrome.runtime usage.
    EXPECT_EQ(1, histogram_tester_.GetBucketCount(
                     "BackForwardCache.HistoryNavigationOutcome."
                     "DisabledForRenderFrameHostReason2",
                     kMessagingBucket));
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that does not allow content scripts to be injected.
class ExtensionBackForwardCacheContentScriptDisabledBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheContentScriptDisabledBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(/*all_extensions_allowed*/ true,
                                             /*allow_content_scripts=*/false) {}
};

// Test that causes non-component extensions to disable back forward cache.
class ExtensionBackForwardCacheExtensionsDisabledBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheExtensionsDisabledBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(/*all_extensions_allowed*/ false,
                                             /*allow_content_scripts*/ false) {}
};

// Tests that a non-component extension that is installed prevents back forward
// cache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheExtensionsDisabledBrowserTest,
                       ScriptDisallowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("trivial_extension")
                                .AppendASCII("extension")));

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

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache
  // since there is an active non-component loaded extension.
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Test content script injection disallow the back forward cache.
// TODO(https://crbug.com/1204751): Very flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_ScriptDisallowed DISABLED_ScriptDisallowed
#else
#define MAYBE_ScriptDisallowed ScriptDisallowed
#endif
IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheContentScriptDisabledBrowserTest,
    MAYBE_ScriptDisallowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::u16string expected_title = u"modified";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  // 1) Navigate to A.
  content::RenderFrameHost* rfh_a =
      ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // 2) Navigate to B.
  content::RenderFrameHost* rfh_b =
      ui_test_utils::NavigateToURL(browser(), url_b);
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache
  // since the active extension injected content_scripts.
  delete_observer_rfh_a.WaitUntilDeleted();
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

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
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

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache.
  delete_observer_rfh_a.WaitUntilDeleted();
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

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       UnloadExtensionFlushCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // Load the extension so we can unload it later.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_css"));
  ASSERT_TRUE(extension);

  // 1) Navigate to A.
  content::RenderFrameHost* rfh_a =
      ui_test_utils::NavigateToURL(browser(), url_a);
  content::RenderFrameDeletedObserver delete_observer_rfh_a(rfh_a);

  // 2) Navigate to B.
  content::RenderFrameHost* rfh_b =
      ui_test_utils::NavigateToURL(browser(), url_b);
  content::RenderFrameDeletedObserver delete_observer_rfh_b(rfh_b);

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now unload the extension after something is in the cache.
  UnloadExtension(extension->id());

  // Expect that `rfh_a` is destroyed as it should be cleared from the cache.
  delete_observer_rfh_a.WaitUntilDeleted();
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       LoadExtensionFlushCache) {
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

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(delete_observer_rfh_a.deleted());
  EXPECT_NE(rfh_a, rfh_b);
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now load the extension after something is in the cache.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  // Expect that `rfh_a` is destroyed as it should be cleared from the cache.
  delete_observer_rfh_a.WaitUntilDeleted();
}

// Test if the chrome.runtime.connect API is called, the page is prevented from
// entering bfcache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectUsage) {
  RunChromeRuntimeTest("chrome.runtime.connect('%s');");
}

// Test if the chrome.runtime.sendMessage API is called, the page is prevented
// from entering bfcache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeSendMessageUsage) {
  RunChromeRuntimeTest(
      "chrome.runtime.sendMessage('%s', 'some "
      "message');");
}

// Test if the chrome.runtime.connect API is called, the page is prevented from
// entering bfcache.
IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheContentScriptDisabledBrowserTest,
    ChromeRuntimeConnectUsage) {
  RunChromeRuntimeTest("chrome.runtime.connect('%s');");

  // Validate also that the not restored reason is `IsolatedWorldScript` due to
  // the extension injecting a content script.
  EXPECT_EQ(
      1,
      histogram_tester_.GetBucketCount(
          "BackForwardCache.HistoryNavigationOutcome.BlocklistedFeature",
          blink::scheduler::WebSchedulerTrackedFeature::kIsolatedWorldScript));
}

}  // namespace extensions
