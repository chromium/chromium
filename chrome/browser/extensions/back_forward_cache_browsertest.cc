// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-shared.h"

namespace extensions {

class ExtensionBackForwardCacheBrowserTest : public ExtensionBrowserTest {
 public:
  explicit ExtensionBackForwardCacheBrowserTest(
      bool all_extensions_allowed = true,
      bool allow_content_scripts = true,
      bool extension_message_support = true,
      std::string blocked_extensions = "")
      : allow_content_scripts_(allow_content_scripts),
        extension_message_support_(extension_message_support) {
    // If `allow_content_scripts` is true then `all_extensions_allowed` must
    // also be true.
    DCHECK(!(allow_content_scripts && !all_extensions_allowed));
    // If `extension_message_support` is true then `allow_content_scripts` and
    // `all_extensions_allowed` must also be true.
    if (extension_message_support)
      DCHECK(allow_content_scripts && all_extensions_allowed);
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kBackForwardCache,
          {{"content_injection_supported",
            allow_content_scripts ? "true" : "false"},
           {"extension_message_supported",
            extension_message_support ? "true" : "false"},
           {"TimeToLiveInBackForwardCacheInSeconds", "3600"},
           {"all_extensions_allowed",
            all_extensions_allowed ? "true" : "false"},
           {"blocked_extensions", blocked_extensions},
           {"ignore_outstanding_network_request_for_testing", "true"}}}},
        {features::kBackForwardCacheMemoryControls});
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void RunChromeRuntimeConnectTest() {
    const Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                          .AppendASCII("content_script"));
    ASSERT_TRUE(extension);

    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
    GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

    // 1) Navigate to A.
    content::RenderFrameHostWrapper rfh_a(
        ui_test_utils::NavigateToURL(browser(), url_a));
    std::u16string expected_title = u"connected";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

    const int kMessagingBucket =
        (static_cast<int>(content::BackForwardCache::DisabledSource::kEmbedder)
         << 16) +
        static_cast<int>(
            extension_message_support_
                ? back_forward_cache::DisabledReasonId::
                      kExtensionSentMessageToCachedFrame
                : back_forward_cache::DisabledReasonId::kExtensionMessaging);

    std::string action = base::StringPrintf(
        R"HTML(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {document.title = m; });
      )HTML",
        extension->id().c_str());
    EXPECT_TRUE(ExecJs(rfh_a.get(), action));

    // 2) Wait for the message port to be connected.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    // Expect that a channel is open.
    EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

    EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                     "BackForwardCache.HistoryNavigationOutcome."
                     "DisabledForRenderFrameHostReason2",
                     kMessagingBucket));
    // 3) Navigate to B.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

    // What happens next depends on whether or not content script is allowed. If
    // it is, then the `rfh_a` should be cached and the channel should still be
    // open. If it isn't, then `rfh_a` and the channel should be deleted.
    if (!allow_content_scripts_) {
      // `rfh_a` should be destroyed, and the channel should be closed.
      ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
      EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
    } else {
      // Expect that `rfh_a` is cached, and the channel is still open.
      EXPECT_EQ(rfh_a->GetLifecycleState(),
                content::RenderFrameHost::LifecycleState::kInBackForwardCache);
      EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

      // 4) Send a message to the port.
      ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
          extension->id(), "port.postMessage('bye');"));

      // `rfh_a` should be destroyed now, and the channel should be closed.
      ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
      EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
    }

    // 5) Go back to A.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents));

    // When extension_message_support_ = true, validate that the not restored
    // reason is `kExtensionSentMessageToCachedFrame` due to a message being
    // sent to an inactive frame. Otherwise, validate that the not restored
    // reason is `ExtensionMessaging` due to extension messages.
    EXPECT_EQ(1, histogram_tester_.GetBucketCount(
                     "BackForwardCache.HistoryNavigationOutcome."
                     "DisabledForRenderFrameHostReason2",
                     kMessagingBucket));
  }

  void ExpectTitleChangeSuccess(const Extension& extension, const char* title) {
    const std::string script = base::StringPrintf(R"(
          chrome.tabs.executeScript({
            code: "document.title='%s'"
          });
        )",
                                                  title);
    ExecuteScriptInBackgroundPageNoWait(extension.id(), script);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::u16string title16(base::UTF8ToUTF16(title));
    content::TitleWatcher title_watcher(web_contents, title16);
    EXPECT_EQ(title16, title_watcher.WaitAndGetTitle());
  }

  void ExpectTitleChangeFail(const Extension& extension) {
    constexpr char kScript[] =
        R"(
          chrome.tabs.executeScript({code: "document.title='fail'"},
            () => {
              if (chrome.runtime.lastError) {
                window.domAutomationController.send(
                  chrome.runtime.lastError.message);
              } else {
                window.domAutomationController.send("Unexpected success");
              }
            });
        )";
    EXPECT_EQ(manifest_errors::kCannotAccessPage,
              ExecuteScriptInBackgroundPage(extension.id(), kScript));

    std::u16string title;
    ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
    EXPECT_NE(u"fail", title);
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
  bool allow_content_scripts_;
  bool extension_message_support_;
};

// Test that does not allow content scripts to be injected.
class ExtensionBackForwardCacheContentScriptDisabledBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheContentScriptDisabledBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(
            /*all_extensions_allowed=*/true,
            /*allow_content_scripts=*/false,
            /*extension_message_support=*/false) {}
};

// Test that does not support extension message.
class ExtensionBackForwardCacheExtensionMessageDisabledBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheExtensionMessageDisabledBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(
            /*all_extensions_allowed=*/true,
            /*allow_content_scripts=*/true,
            /*extension_message_support=*/false) {}
};

// Test that causes non-component extensions to disable back forward cache.
class ExtensionBackForwardCacheExtensionsDisabledBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheExtensionsDisabledBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(
            /*all_extensions_allowed=*/false,
            /*allow_content_scripts=*/false,
            /*extension_message_support=*/false) {}
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
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache
  // since there is an active non-component loaded extension.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

// Test content script injection disallow the back forward cache.
IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheContentScriptDisabledBrowserTest,
    ScriptDisallowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::u16string expected_title = u"modified";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache
  // since the active extension injected content_scripts.
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest, ScriptAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper rfh_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
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
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache.
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest, CSSAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper rfh_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
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
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper rfh_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now unload the extension after something is in the cache.
  UnloadExtension(extension->id());

  // Expect that `rfh_a` is destroyed as it should be cleared from the cache.
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       LoadExtensionFlushCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper rfh_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now load the extension after something is in the cache.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  // Expect that `rfh_a` is destroyed as it should be cleared from the cache.
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

// Test if the chrome.runtime.connect API is called, the page is prevented from
// entering bfcache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectUsage) {
  RunChromeRuntimeConnectTest();
}

// Test that we correctly clear the bfcache disable reasons on a same-origin
// cross document navigation for a document with an active channel, allowing
// the frame to be bfcached subsequently.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectUsageInIframe) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/iframe.html");
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper primary_rfh(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  content::RenderFrameHost* child = ChildFrameAt(primary_rfh.get(), 0);

  std::string action = base::StringPrintf(
      R"HTML(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {window.top.document.title = m; });
      )HTML",
      extension->id().c_str());
  ASSERT_TRUE(ExecJs(child, action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Expect that a channel is open.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate the iframe.
  GURL iframe_url = embedded_test_server()->GetURL("a.com", "/title2.html");
  EXPECT_TRUE(NavigateToURLFromRenderer(child, iframe_url));

  EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());

  // 4) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 5) Expect that A is in the back forward cache.
  EXPECT_FALSE(primary_rfh.IsDestroyed());
  EXPECT_EQ(primary_rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test if the chrome.runtime.sendMessage API is called, the page is allowed
// to enter the bfcache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeSendMessageUsage) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  std::u16string expected_title = u"sent";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  std::string action =
      R"HTML(
        chrome.runtime.sendMessage('%s', 'some message',
          () => { document.title = 'sent'});
      )HTML";
  EXPECT_TRUE(ExecJs(rfh_a.get(), base::StringPrintf(action.c_str(),
                                                     extension->id().c_str())));

  // 2) Wait until the sendMessage has completed.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Expect that no channel is open.
  EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) Expect that A is in the back forward cache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 5) Ensure that the runtime.onConnect listener in the restored page still
  // works.
  constexpr char kScript[] =
      R"HTML(
      var p;
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        p.onMessage.addListener(
         (m) => {window.domAutomationController.send(m)}
        );
      });
    )HTML";
  EXPECT_EQ("connected",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));
}

// Test if the chrome.runtime.connect is called then disconnected, the page is
// allowed to enter the bfcache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectDisconnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";
  auto title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  std::string action = base::StringPrintf(
      R"HTML(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {document.title = m; });
      )HTML",
      extension->id().c_str());
  EXPECT_TRUE(ExecJs(rfh_a.get(), action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  expected_title = u"disconnect";
  title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_TRUE(ExecJs(rfh_a.get(),
                     R"HTML(
        p.onDisconnect.addListener((m) => {document.title = 'disconnect';});
        p.postMessage('disconnect');
      )HTML"));

  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // Expect that the channel is closed.
  EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) Expect that A is in the back forward cache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test if the chrome.runtime.connect is called then disconnected, the page is
// not allowed to enter the bfcache if extension_message_supported = false.
IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheExtensionMessageDisabledBrowserTest,
    ChromeRuntimeConnectDisconnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  constexpr int kMessagingBucket =
      (static_cast<int>(content::BackForwardCache::DisabledSource::kEmbedder)
       << 16) +
      static_cast<int>(
          back_forward_cache::DisabledReasonId::kExtensionMessaging);

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";
  auto title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  std::string action = base::StringPrintf(
      R"HTML(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {document.title = m;});
      )HTML",
      extension->id().c_str());
  EXPECT_TRUE(ExecJs(rfh_a.get(), action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
  expected_title = u"disconnect";
  title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_TRUE(ExecJs(rfh_a.get(),
                     R"HTML(
        p.onDisconnect.addListener((m) => {document.title = 'disconnect';});
        p.postMessage('disconnect');
      )HTML"));

  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // Expect that the channel is closed.
  EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());

  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   "BackForwardCache.HistoryNavigationOutcome."
                   "DisabledForRenderFrameHostReason2",
                   kMessagingBucket));

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_TRUE(
      WaitForLoadStop(browser()->tab_strip_model()->GetActiveWebContents()));

  // 4) Expect that `rfh_a` is deleted.
  ASSERT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());

  // 5) Go back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Validate that the not restored reason is `ExtensionMessaging`
  // due to extension_message_supported = false.
  EXPECT_EQ(1, histogram_tester_.GetBucketCount(
                   "BackForwardCache.HistoryNavigationOutcome."
                   "DisabledForRenderFrameHostReason2",
                   kMessagingBucket));
}

// Test if the chrome.tabs.connect is called and then the page is navigated,
// the page is allowed to enter the bfcache, but if the extension tries to send
// it a message the page will be evicted.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";

  constexpr char kScript[] =
      R"HTML(
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        // Save a "global" reference to the port so it can be used by the test
        // later.
        port = p;
        p.onMessage.addListener(
         (m) => {window.domAutomationController.send(m)}
        );
      });
    )HTML";
  EXPECT_EQ("connected",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));

  // Expect that a channel is open.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Expect that `rfh_a` is cached, and the channel is still open.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 4) Send a message to the port.
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(extension->id(),
                                                  "port.postMessage('bye');"));

  // Expect that `rfh_a` is destroyed, since the message should cause it to be
  // evicted, and that the channel is closed.
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
  EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
}

// Test that after caching and restoring a page, long-lived ports still work.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnectChannelWorksAfterRestore) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title_connected = u"connected";
  content::TitleWatcher title_watcher_connected(
      browser()->tab_strip_model()->GetActiveWebContents(),
      expected_title_connected);

  EXPECT_EQ(MessageService::Get(profile())->GetChannelCountForTest(), 0u);

  std::string action = base::StringPrintf(
      R"HTML(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {
          document.title = m;
        });
      )HTML",
      extension->id().c_str());
  ASSERT_TRUE(ExecJs(rfh_a.get(), action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title_connected,
            title_watcher_connected.WaitAndGetTitle());

  EXPECT_EQ(MessageService::Get(profile())->GetChannelCountForTest(), 1u);

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  EXPECT_EQ(MessageService::Get(profile())->GetChannelCountForTest(), 1u);

  // Expect that `rfh_a` is cached.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Navigate back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents));

  // Verify that `rfh_a` is the active frame again.
  EXPECT_TRUE(rfh_a->GetLifecycleState() ==
              content::RenderFrameHost::LifecycleState::kActive);

  // 5) Post a message to the frame.
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
      extension->id(), "port.postMessage('restored');"));

  // Verify that the message was received properly.
  content::TitleWatcher title_watcher_restored(
      browser()->tab_strip_model()->GetActiveWebContents(), u"restored");
  EXPECT_EQ(u"restored", title_watcher_restored.WaitAndGetTitle());
}

// Test if the chrome.tabs.connect is called then disconnected, the page is
// allowed to enter the bfcache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnectDisconnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";

  constexpr char kScript[] =
      R"HTML(
      var p;
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        p.onMessage.addListener(
         (m) => {window.domAutomationController.send(m)}
        );
      });
    )HTML";
  EXPECT_EQ("connected",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));

  constexpr char kDisconnectScript[] =
      R"HTML(
      p.postMessage('disconnect');
      p.onDisconnect.addListener(() => {
        window.domAutomationController.send('disconnect')
      });
    )HTML";
  EXPECT_EQ("disconnect",
            ExecuteScriptInBackgroundPage(extension->id(), kDisconnectScript));

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) Expect that A is in the back forward cache.
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test if the chrome.runtime.connect API is called, the page is prevented from
// entering bfcache.
IN_PROC_BROWSER_TEST_F(
    ExtensionBackForwardCacheContentScriptDisabledBrowserTest,
    ChromeRuntimeConnectUsage) {
  RunChromeRuntimeConnectTest();

  // Validate also that the not restored reason is `IsolatedWorldScript` due to
  // the extension injecting a content script.
  EXPECT_EQ(
      1,
      histogram_tester_.GetBucketCount(
          "BackForwardCache.HistoryNavigationOutcome.BlocklistedFeature",
          blink::scheduler::WebSchedulerTrackedFeature::kInjectedJavascript));
}
// Tests sending a message to all frames does not send it to back-forward
// cached frames.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       MessageSentToAllFramesDoesNotSendToBackForwardCache) {
  const Extension* extension = extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("background_page"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper rfh_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  ASSERT_FALSE(rfh_a.IsDestroyed());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  std::u16string expected_title = u"foo";
  auto title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  constexpr char kScript[] =
      R"HTML(
      chrome.tabs.executeScript({allFrames: true, code: "document.title='foo'"})
    )HTML";
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(extension->id(), kScript));

  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // `rfh_a` should still be in the cache.
  ASSERT_FALSE(rfh_a.IsDestroyed());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Expect the original title when going back to A.
  expected_title = u"Title Of Awesomeness";
  title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  // Go back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // `rfh_b` should still be in the cache.
  ASSERT_FALSE(rfh_b.IsDestroyed());
  EXPECT_EQ(rfh_b->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now go forward to B, and expect that it is what was set before it
  // went into the back forward cache.
  expected_title = u"foo";
  title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  web_contents->GetController().GoForward();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());
}

// Tests sending a message to specific frame that is in the back forward cache
// fails.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       MessageSentToCachedIdFails) {
  const Extension* extension = extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("background_page"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/iframe_blank.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  content::RenderFrameHostWrapper iframe(ChildFrameAt(rfh_a.get(), 0));
  ASSERT_TRUE(iframe.get());

  // Cache the iframe's frame tree node id to send it a message later.
  int iframe_frame_tree_node_id = iframe->GetFrameTreeNodeId();

  // 2) Navigate to B.
  content::RenderFrameHostWrapper rfh_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_NE(rfh_a.get(), rfh_b.get());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  std::u16string expected_title = u"foo";
  auto title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  constexpr char kScript[] =
      R"HTML(
        chrome.tabs.executeScript({frameId: %d,
                                   code: "document.title='foo'",
                                   matchAboutBlank: true
                                  }, (e) => {
          window.domAutomationController.send(chrome.runtime.lastError ? 'false'
        : 'true')});
      )HTML";
  EXPECT_EQ("false",
            ExecuteScriptInBackgroundPage(
                extension->id(),
                base::StringPrintf(kScript, iframe_frame_tree_node_id)));
  // Go back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Re-execute the script.
  EXPECT_EQ("true",
            ExecuteScriptInBackgroundPage(
                extension->id(),
                base::StringPrintf(kScript, iframe_frame_tree_node_id)));
}

// TODO(crbug.com/1317431): WebSQL does not work on Fuchsia.
// TODO(crbug.com/1382285): Flaky on Mac.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC)
#define MAYBE_StorageCallbackEvicts DISABLED_StorageCallbackEvicts
#else
#define MAYBE_StorageCallbackEvicts StorageCallbackEvicts
#endif
// Test that running extensions message dispatching via a ScriptContext::ForEach
// for back forward cached pages causes eviction of that RenderFrameHost.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       MAYBE_StorageCallbackEvicts) {
  const Extension* extension = extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script_storage"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B. Ensure that |rfh_a| is in back/forward cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  // Validate that the eviction due to JavaScript execution has not happened.
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   "BackForwardCache.Eviction.Renderer",
                   blink::mojom::RendererEvictionReason::kJavaScriptExecution));

  // 3) Navigate back to A and make sure that the callback is called after
  // restore.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("called",
            EvalJs(rfh_a.get(), "document.getElementById('callback').value;"));
}

// Test that allows all extensions but disables bfcache in the presence of a few
// blocked ones.
class ExtensionBackForwardCacheBlockedExtensionBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheBlockedExtensionBrowserTest()
      : ExtensionBackForwardCacheBrowserTest(
            /*all_extensions_allowed=*/true,
            /*allow_content_scripts=*/true,
            /*extension_message_support=*/true,
            /*blocked_extensions=*/
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa,mockepjebcnmhmhcahfddgfcdgkdifnc,"
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb") {}
};

// Tests that a blocked extension that is installed prevents back forward
// cache.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBlockedExtensionBrowserTest,
                       ScriptDisallowed) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("trivial_extension")
                        .AppendASCII("extension.crx"));
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension->id(), "mockepjebcnmhmhcahfddgfcdgkdifnc");

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Expect that `rfh_a` is destroyed as it wouldn't be placed in the cache
  // since there is a blocked feature flag with id
  // 'mockepjebcnmhmhcahfddgfcdgkdifnc'.
  EXPECT_TRUE(rfh_a.WaitUntilRenderFrameDeleted());
}

// Test that ensures the origin restriction declared on the extension
// manifest.json is properly respected even when BFCache is involved.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest, TabsOrigin) {
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("correct_origin"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  ExpectTitleChangeSuccess(*extension, "first nav");

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  ExpectTitleChangeFail(*extension);

  // 3) Go back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  std::u16string title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &title));
  ASSERT_EQ(title, u"first nav");
  ExpectTitleChangeSuccess(*extension, "restore nav");
}

// Test that ensures the content scripts only execute once on a back/forward
// cached page.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ContentScriptsRunOnlyOnce) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script_stages")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  std::u16string expected_title = u"document_idle";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Verify that the content scripts have been run (the 'stage' element
  // is created by the content script running at 'document_start" and
  // populated whenever the content script run at 'document_start',
  // 'document_end', or 'document_idle').
  EXPECT_EQ("document_start/document_end/document_idle/page_show/",
            EvalJs(rfh_a.get(), "document.getElementById('stage').value;"));

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 3) Go back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Verify that the content scripts have not run again and that the
  // 'stage' element has the appended a page_hide/page_show to its list.
  EXPECT_EQ(
      "document_start/document_end/document_idle/page_show/page_hide/"
      "page_show/",
      EvalJs(rfh_a.get(), "document.getElementById('stage').value;"));
}

// Test that an activeTab permission temporarily granted to an extension for a
// page does not revive when the BFCache entry is restored.
IN_PROC_BROWSER_TEST_F(ExtensionBackForwardCacheBrowserTest,
                       ActiveTabPermissionRevoked) {
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("active_tab"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // Grant the activeTab permission.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension.get(), /* grant_tab_permissions=*/true);

  ExpectTitleChangeSuccess(*extension, "changed_title");

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `rfh_a` is in the cache.
  EXPECT_FALSE(rfh_a.IsDestroyed());
  EXPECT_EQ(rfh_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Extension should no longer be able to change title, since the permission
  // should be revoked with a cross-site navigation.
  ExpectTitleChangeFail(*extension);

  // 3) Go back to A.
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Extension should no longer be able to change title, since the permission
  // should not revive with BFCache navigation to a.com.
  ExpectTitleChangeFail(*extension);
}

}  // namespace extensions
