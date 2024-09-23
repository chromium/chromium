// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/back_forward_cache.h"

#include <string>
#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/messaging/message_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/mojom/navigation/renderer_eviction_reason.mojom-shared.h"

namespace extensions {

using ContextType = ExtensionBrowserTest::ContextType;

struct TestParams {
  bool enable_disconnect_message_port_on_bfcache;
  ContextType context_type;
};

class ExtensionBackForwardCacheBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<TestParams> {
 public:
  ExtensionBackForwardCacheBrowserTest()
      : ExtensionBrowserTest(GetParam().context_type) {
    auto enabled_features =
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{features::kBackForwardCache, {}}});
    auto disabled_features =
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting();
    if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
      enabled_features.push_back(
          {features::kDisconnectExtensionMessagePortWhenPageEntersBFCache, {}});
    } else {
      disabled_features.push_back(
          features::kDisconnectExtensionMessagePortWhenPageEntersBFCache);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  bool IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled() {
    return GetParam().enable_disconnect_message_port_on_bfcache;
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  content::RenderFrameHost* current_main_frame_host() {
    return web_contents()->GetPrimaryMainFrame();
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
    content::RenderFrameHostWrapper render_frame_host_a(
        ui_test_utils::NavigateToURL(browser(), url_a));
    std::u16string expected_title = u"connected";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

    const int kMessagingBucket =
        (static_cast<int>(content::BackForwardCache::DisabledSource::kEmbedder)
         << 16) +
        static_cast<int>(back_forward_cache::DisabledReasonId::
                             kExtensionSentMessageToCachedFrame);

    std::string action = base::StringPrintf(
        R"HTML(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {document.title = m; });
      )HTML",
        extension->id().c_str());
    EXPECT_TRUE(ExecJs(render_frame_host_a.get(), action));

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

    // Expect that `render_frame_host_a` is cached.
    EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
              content::RenderFrameHost::LifecycleState::kInBackForwardCache);

    // The channel should remain open if
    // DisconnectExtensionMessagePortWhenPageEntersBFCache is disabled.
    if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
      EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
    } else {
      EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());
      // Send a message to the port.
      ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
          extension->id(), "port.postMessage('bye');"));

      // `render_frame_host_a` should be destroyed now, and the channel should
      // be closed.
      ASSERT_TRUE(render_frame_host_a.WaitUntilRenderFrameDeleted());
      EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
    }

    // 4) Go back to A.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->GetController().GoBack();
    EXPECT_TRUE(WaitForLoadStop(web_contents));

    int expected_count = 0;
    if (!IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
      // If `DisconnectExtensionMessagePortWhenPageEntersBFCache` is
      // disabled, validate that the not restored reason is
      // `kExtensionSentMessageToCachedFrame` due to a message being sent to an
      // inactive frame.
      expected_count = 1;
    }
    EXPECT_EQ(expected_count, histogram_tester_.GetBucketCount(
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
    static constexpr char kScript[] =
        R"(
          chrome.tabs.executeScript({code: "document.title='fail'"},
            () => {
              if (chrome.runtime.lastError) {
                chrome.test.sendScriptResult(
                  chrome.runtime.lastError.message);
              } else {
                chrome.test.sendScriptResult("Unexpected success");
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
};

// These tests use chrome.tabs.executeScript, so the SW versions of the tests
// must still be run with MV2. See crbug.com/332328868.
INSTANTIATE_TEST_SUITE_P(EventPageAndFalse,
                         ExtensionBackForwardCacheBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = false,
                             .context_type = ContextType::kEventPage}));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerAndFalse,
                         ExtensionBackForwardCacheBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = false,
                             .context_type = ContextType::kServiceWorkerMV2}));
INSTANTIATE_TEST_SUITE_P(EventPageAndTrue,
                         ExtensionBackForwardCacheBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = true,
                             .context_type = ContextType::kEventPage}));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerAndTrue,
                         ExtensionBackForwardCacheBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = true,
                             .context_type = ContextType::kServiceWorkerMV2}));

IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest, ScriptAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_script")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper render_frame_host_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_NE(render_frame_host_a.get(), render_frame_host_b.get());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest, CSSAllowed) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper render_frame_host_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_NE(render_frame_host_a.get(), render_frame_host_b.get());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
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
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper render_frame_host_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_NE(render_frame_host_a.get(), render_frame_host_b.get());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now unload the extension after something is in the cache.
  UnloadExtension(extension->id());

  // Expect that `render_frame_host_a` is destroyed as it should be cleared from
  // the cache.
  EXPECT_TRUE(render_frame_host_a.WaitUntilRenderFrameDeleted());
}

IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       LoadExtensionFlushCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper render_frame_host_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_NE(render_frame_host_a.get(), render_frame_host_b.get());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // Now load the extension after something is in the cache.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                                .AppendASCII("content_css")));

  // Expect that `render_frame_host_a` is destroyed as it should be cleared from
  // the cache.
  EXPECT_TRUE(render_frame_host_a.WaitUntilRenderFrameDeleted());
}

// Test if the chrome.runtime.connect API is called, the page is prevented from
// entering bfcache.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectUsage) {
  RunChromeRuntimeConnectTest();
}

// Test that we correctly clear the bfcache disable reasons on a same-origin
// cross document navigation for a document with an active channel, allowing
// the frame to be bfcached subsequently.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectUsageInIframeWithIframeNavigation) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/iframe.html");
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper primary_render_frame_host(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  content::RenderFrameHost* child =
      ChildFrameAt(primary_render_frame_host.get(), 0);

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
  EXPECT_FALSE(primary_render_frame_host.IsDestroyed());
  EXPECT_EQ(primary_render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test that the page can enter BFCache with an active channel created from the
// iframe.
IN_PROC_BROWSER_TEST_P(
    ExtensionBackForwardCacheBrowserTest,
    ChromeRuntimeConnectUsageInIframeWithoutIframeNavigation) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = embedded_test_server()->GetURL("a.com", "/iframe.html");
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper primary_render_frame_host(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  content::RenderFrameHost* child =
      ChildFrameAt(primary_render_frame_host.get(), 0);

  std::string action = base::StringPrintf(
      R"JS(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {window.top.document.title = m; });
      )JS",
      extension->id().c_str());
  ASSERT_TRUE(ExecJs(child, action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Expect that a channel is open.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B, and the channel is still open if
  // `DisconnectExtensionMessagePortWhenPageEntersBFCache` is not enabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
  } else {
    EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());
  }

  // 4) Expect that A is in the back forward cache.
  EXPECT_FALSE(primary_render_frame_host.IsDestroyed());
  EXPECT_EQ(primary_render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test that the page can enter BFCache with an active channel that's created
// from the extension background with two receivers from different frames.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnectWithMultipleReceivers) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script_all_frames"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper primary_render_frame_host(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Create channel from the extension background.
  static constexpr char kScript[] =
      R"JS(
      var p;
      var countConnected = 0;
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        p.onMessage.addListener(
         (m) => {
          if (m == 'connected') {
            countConnected++;
            if (countConnected == 2) {
              chrome.test.sendScriptResult('connected twice');
            }
          }
        });
      });
    )JS";

  // The background should receives two "connected" messages from different
  // frames.
  EXPECT_EQ("connected twice",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));
  // Even though there are two ports from the receiver end, there is still one
  // channel.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) Expect that A is in the back forward cache.
  EXPECT_EQ(primary_render_frame_host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
  } else {
    // When `DisconnectExtensionMessagePortWhenPageEntersBFCache` is not
    // enabled, the channel should still be active.
    EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());
  }
}

// Test if the chrome.runtime.sendMessage API is called, the page is allowed
// to enter the bfcache.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeSendMessageUsage) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  std::u16string expected_title = u"sent";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  static constexpr char kAction[] =
      R"HTML(
        chrome.runtime.sendMessage('%s', 'some message',
          () => { document.title = 'sent'});
      )HTML";
  EXPECT_TRUE(ExecJs(render_frame_host_a.get(),
                     base::StringPrintf(kAction, extension->id().c_str())));

  // 2) Wait until the sendMessage has completed.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Expect that no channel is open.
  EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) Expect that A is in the back forward cache.
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 5) Ensure that the runtime.onConnect listener in the restored page still
  // works.
  static constexpr char kScript[] =
      R"HTML(
      var p;
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        p.onMessage.addListener(
         (m) => {chrome.test.sendScriptResult(m)}
        );
      });
    )HTML";
  EXPECT_EQ("connected",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));
}

// Test if the chrome.runtime.connect is called then disconnected, the page is
// allowed to enter the bfcache.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeRuntimeConnectDisconnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
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
  EXPECT_TRUE(ExecJs(render_frame_host_a.get(), action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  expected_title = u"disconnect";
  title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_TRUE(ExecJs(render_frame_host_a.get(),
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
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test if the chrome.tabs.connect is called and then the page is navigated,
// the page is allowed to enter the bfcache, but if the extension tries to send
// it a message the page will be evicted.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";

  static constexpr char kScript[] =
      R"HTML(
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        // Save a "global" reference to the port so it can be used by the test
        // later.
        port = p;
        p.onMessage.addListener(
         (m) => {chrome.test.sendScriptResult(m)}
        );
      });
    )HTML";
  EXPECT_EQ("connected",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));

  // Expect that a channel is open.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Expect that `render_frame_host_a` is cached, and the channel is still open
  // only if `DisconnectExtensionMessagePortWhenPageEntersBFCache` is disabled.
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
  } else {
    EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());
    // Send a message to the port.
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(), "port.postMessage('bye');"));
    // Expect that `render_frame_host_a` is destroyed, since the message should
    // cause it to be evicted, and that the channel is closed.
    EXPECT_TRUE(render_frame_host_a.WaitUntilRenderFrameDeleted());
    EXPECT_EQ(0u, MessageService::Get(profile())->GetChannelCountForTest());
  }
}

// Test that after caching and restoring a page, long-lived ports still work.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnectChannelWorksAfterRestore) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
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
  ASSERT_TRUE(ExecJs(render_frame_host_a.get(), action));

  // 2) Wait for the message port to be connected.
  EXPECT_EQ(expected_title_connected,
            title_watcher_connected.WaitAndGetTitle());

  EXPECT_EQ(MessageService::Get(profile())->GetChannelCountForTest(), 1u);

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    EXPECT_EQ(MessageService::Get(profile())->GetChannelCountForTest(), 0u);
  } else {
    EXPECT_EQ(MessageService::Get(profile())->GetChannelCountForTest(), 1u);
  }

  // Expect that `render_frame_host_a` is cached.
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 4) Navigate back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents));

  // Verify that `render_frame_host_a` is the active frame again.
  EXPECT_TRUE(render_frame_host_a->GetLifecycleState() ==
              content::RenderFrameHost::LifecycleState::kActive);

  // 5) Post a message to the frame. Note that we shouldn't do this when
  // `DisconnectExtensionMessagePortWhenPageEntersBFCache` is enabled, because
  // the port has already been closed.
  if (!IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(
        extension->id(), "port.postMessage('restored');"));

    // Verify that the message was received properly.
    content::TitleWatcher title_watcher_restored(
        browser()->tab_strip_model()->GetActiveWebContents(), u"restored");
    EXPECT_EQ(u"restored", title_watcher_restored.WaitAndGetTitle());
  }
}

// Test if the chrome.tabs.connect is called then disconnected, the page is
// allowed to enter the bfcache.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ChromeTabsConnectDisconnect) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";

  static constexpr char kScript[] =
      R"HTML(
      var p;
      chrome.tabs.query({}, (t) => {
        p = chrome.tabs.connect(t[0].id);
        p.onMessage.addListener(
         (m) => {chrome.test.sendScriptResult(m)}
        );
      });
    )HTML";
  EXPECT_EQ("connected",
            ExecuteScriptInBackgroundPage(extension->id(), kScript));

  static constexpr char kDisconnectScript[] =
      R"HTML(
      p.postMessage('disconnect');
      p.onDisconnect.addListener(() => {
        chrome.test.sendScriptResult('disconnect')
      });
    )HTML";
  EXPECT_EQ("disconnect",
            ExecuteScriptInBackgroundPage(extension->id(), kDisconnectScript));

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) Expect that A is in the back forward cache.
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
}

// Test that the extension background receives `disconnect` event if the
// channel is closed after the page enters BFCache when
// `DisconnectExtensionMessagePortWhenPageEntersBFCache` is enabled.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ExtensionBackgroundOnDisconnectEvent) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("back_forward_cache")
          .AppendASCII("content_script_with_background_disconnect_listener"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper rfh(
      ui_test_utils::NavigateToURL(browser(), url_a));
  std::u16string expected_title = u"connected";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  std::string connectScript = base::StringPrintf(
      R"JS(
        var p = chrome.runtime.connect('%s');
        p.onMessage.addListener((m) => {document.title = m; });
      )JS",
      extension->id().c_str());
  ASSERT_TRUE(ExecJs(rfh.get(), connectScript));

  // 2) Wait for the message port to be connected.
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Expect that a channel is open.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());

  // 3) Navigate to B, and the channel is still open when the
  // `DisconnectExtensionMessagePortWhenPageEntersBFCache` is disabled, and
  // closed when it's enabled.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(
      IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled() ? 0u : 1u,
      MessageService::Get(profile())->GetChannelCountForTest());

  // 4) Expect that A is in the back forward cache.
  ASSERT_FALSE(rfh.IsDestroyed());
  EXPECT_EQ(rfh->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  // 5) Expect that the `disconnect` event is dispatched to the
  // background if
  // `IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled` is enabled.
  constexpr char kCheckDisconnectCountScript[] =
      R"JS(chrome.test.sendScriptResult(disconnectCount))JS";
  EXPECT_EQ(
      IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled() ? 1 : 0,
      ExecuteScriptInBackgroundPage(extension->id(),
                                    kCheckDisconnectCountScript));
}

// Tests sending a message to all frames does not send it to back-forward
// cached frames.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       MessageSentToAllFramesDoesNotSendToBackForwardCache) {
  const Extension* extension = extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("background_page"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title2.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B.
  content::RenderFrameHostWrapper render_frame_host_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  ASSERT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  std::u16string expected_title = u"foo";
  auto title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  static constexpr char kScript[] =
      R"HTML(
      chrome.tabs.executeScript({allFrames: true, code: "document.title='foo'"})
    )HTML";
  ASSERT_TRUE(ExecuteScriptInBackgroundPageNoWait(extension->id(), kScript));

  EXPECT_EQ(expected_title, title_watcher->WaitAndGetTitle());

  // `render_frame_host_a` should still be in the cache.
  ASSERT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
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

  // `render_frame_host_b` should still be in the cache.
  ASSERT_FALSE(render_frame_host_b.IsDestroyed());
  EXPECT_EQ(render_frame_host_b->GetLifecycleState(),
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
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       MessageSentToCachedIdFails) {
  const Extension* extension = extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("background_page"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/iframe_blank.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  content::RenderFrameHostWrapper iframe(
      ChildFrameAt(render_frame_host_a.get(), 0));
  ASSERT_TRUE(iframe.get());

  // Cache the iframe's frame tree node id to send it a message later.
  content::FrameTreeNodeId iframe_frame_tree_node_id =
      iframe->GetFrameTreeNodeId();

  // 2) Navigate to B.
  content::RenderFrameHostWrapper render_frame_host_b(
      ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_NE(render_frame_host_a.get(), render_frame_host_b.get());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);

  std::u16string expected_title = u"foo";
  auto title_watcher = std::make_unique<content::TitleWatcher>(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);

  static constexpr char kScript[] =
      R"HTML(
        chrome.tabs.executeScript({frameId: %d,
                                   code: "document.title='foo'",
                                   matchAboutBlank: true
                                  }, (e) => {
          chrome.test.sendScriptResult(chrome.runtime.lastError ? 'false'
        : 'true')});
      )HTML";
  EXPECT_EQ("false", ExecuteScriptInBackgroundPage(
                         extension->id(),
                         base::StringPrintf(
                             kScript, iframe_frame_tree_node_id.value())));
  // Go back to A.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  // Re-execute the script.
  EXPECT_EQ("true", ExecuteScriptInBackgroundPage(
                        extension->id(),
                        base::StringPrintf(kScript,
                                           iframe_frame_tree_node_id.value())));
}

// Test that running extensions message dispatching via a ScriptContext::ForEach
// for back forward cached pages causes eviction of that RenderFrameHost.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       StorageCallbackEvicts) {
  const Extension* extension = extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script_storage"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));

  // 1) Navigate to A and wait until the extension's content script has
  // executed.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Navigate to B. Ensure that |render_frame_host_a| is in back/forward
  // cache.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kInBackForwardCache);
  // Validate that the eviction due to JavaScript execution has not happened.
  EXPECT_EQ(0, histogram_tester_.GetBucketCount(
                   "BackForwardCache.Eviction.Renderer",
                   blink::mojom::RendererEvictionReason::kJavaScriptExecution));

  // 3) Navigate back to A and make sure that the callback is called after
  // restore.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  // Check that the page was cached.
  ASSERT_EQ(render_frame_host_a.get(), web_contents()->GetPrimaryMainFrame());

  // Wait for the content script to run.
  content::DOMMessageQueue dom_message_queue(web_contents());
  std::string dom_message;
  ASSERT_TRUE(dom_message_queue.WaitForMessage(&dom_message));
  ASSERT_EQ("\"event handler ran\"", dom_message);

  // Verify that the callback was called.
  EXPECT_EQ("called", EvalJs(render_frame_host_a.get(),
                             "document.getElementById('callback').value;"));
}

// Test that ensures the origin restriction declared on the extension
// manifest.json is properly respected even when BFCache is involved.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest, TabsOrigin) {
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("correct_origin"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  ExpectTitleChangeSuccess(*extension, "first nav");

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
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
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
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
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Verify that the content scripts have been run (the 'stage' element
  // is created by the content script running at 'document_start" and
  // populated whenever the content script run at 'document_start',
  // 'document_end', or 'document_idle').
  EXPECT_EQ("document_start/document_end/document_idle/page_show/",
            EvalJs(render_frame_host_a.get(),
                   "document.getElementById('stage').value;"));

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
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
      EvalJs(render_frame_host_a.get(),
             "document.getElementById('stage').value;"));
}

// Test that an activeTab permission temporarily granted to an extension for a
// page does not revive when the BFCache entry is restored.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheBrowserTest,
                       ActiveTabPermissionRevoked) {
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("active_tab"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // Grant the activeTab permission.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExtensionActionRunner::GetForWebContents(web_contents)
      ->RunAction(extension.get(), /* grant_tab_permissions=*/true);

  ExpectTitleChangeSuccess(*extension, "changed_title");

  // 2) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // Ensure that `render_frame_host_a` is in the cache.
  EXPECT_FALSE(render_frame_host_a.IsDestroyed());
  EXPECT_EQ(render_frame_host_a->GetLifecycleState(),
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

// This subclass adds some necessary setup for testing the BFCache metrics
// reported by the extensions.
class ExtensionBackForwardCacheMetricsBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBackForwardCacheBrowserTest::SetUpOnMainThread();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    // Enable extension sync, otherwise the new source url entry will be
    // dropped.
    test_ukm_recorder_->SetIsWebstoreExtensionCallback(
        base::BindRepeating([](std::string_view) { return true; }));
  }

 protected:
  ukm::TestUkmRecorder* test_ukm_recorder() { return test_ukm_recorder_.get(); }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

INSTANTIATE_TEST_SUITE_P(EventPageAndFalse,
                         ExtensionBackForwardCacheMetricsBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = false,
                             .context_type = ContextType::kEventPage}));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerAndFalse,
                         ExtensionBackForwardCacheMetricsBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = false,
                             .context_type = ContextType::kServiceWorker}));
INSTANTIATE_TEST_SUITE_P(EventPageAndTrue,
                         ExtensionBackForwardCacheMetricsBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = true,
                             .context_type = ContextType::kEventPage}));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerAndTrue,
                         ExtensionBackForwardCacheMetricsBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = true,
                             .context_type = ContextType::kServiceWorker}));

namespace {

// Convert the given source and reason into metric value that is used for metric
// testing. This follows the implementation of
// `content::BackForwardCacheMetrics::MetricValue`.
// See the comments from `content::BackForwardCache::DisabledSource` also.
constexpr int ToBackForwardCacheDisabledReasonMetricValue(
    content::BackForwardCache::DisabledSource source,
    back_forward_cache::DisabledReasonId reason) {
  return (static_cast<int>(source) << 16) + static_cast<int>(reason);
}

}  // namespace

// Test when `DisconnectExtensionMessagePortWhenPageEntersBFCache` is disabled,
// if the extension sends message to a cached document, the document is not
// allowed to enter the back/forward cache, and the
// `BackForwardCacheDisabledForRenderFrameHostReason` metric will be recorded
// for the document URL and the extension URL.
// It also tests the case when the same extension triggers the disabling twice
// in different navigations, the metrics should be recorded under different
// source ids.
IN_PROC_BROWSER_TEST_P(
    ExtensionBackForwardCacheMetricsBrowserTest,
    BFCacheMetricsRecordedIfExtensionSendsMessageToCachedFrame) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script_message_on_pagehide"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  // 1) Navigate to A.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_a));
  content::RenderFrameHostWrapper render_frame_host_a(
      current_main_frame_host());

  // 2) Wait for the extension to be successfully loaded.
  const char16_t kTitleModified[] = u"modified";
  ASSERT_EQ(
      kTitleModified,
      content::TitleWatcher(web_contents(), kTitleModified).WaitAndGetTitle());

  // 3) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 4) If `kDisconnectExtensionMessagePortWhenPageEntersBFCache` is not
  // enabled, wait for A to be deleted since back/forward cache will be disabled
  // because the loaded extension is attempting to send messages to the cached
  // page A.
  if (!IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    ASSERT_TRUE(render_frame_host_a.WaitUntilRenderFrameDeleted());
  }

  // 5) Go back to A.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Expect that metrics are recorded properly in `test_ukm_recorder()`.
  constexpr int kExtensionSentMessageToCachedFrame =
      ToBackForwardCacheDisabledReasonMetricValue(
          content::BackForwardCache::DisabledSource::kEmbedder,
          back_forward_cache::DisabledReasonId::
              kExtensionSentMessageToCachedFrame);

  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::BackForwardCacheDisabledForRenderFrameHostReason::
          kEntryName);

  if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    // If `DisconnectExtensionMessagePortWhenPageEntersBFCache` is enabled, the
    // page will be restored from BFCache.
    ASSERT_EQ(0u, entries.size());
  } else {
    // There should be two entries, one for the document URL and one for the
    // extension URL.
    ASSERT_EQ(2u, entries.size());

    std::vector<GURL> entry_urls;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      auto* src = test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      EXPECT_TRUE(src)
          << "The recorded UKM source id should have a source URL registered.";

      entry_urls.push_back(src->url());
      test_ukm_recorder()->ExpectEntryMetric(
          entry,
          ukm::builders::BackForwardCacheDisabledForRenderFrameHostReason::
              kReason2Name,
          kExtensionSentMessageToCachedFrame);
    }

    EXPECT_THAT(entry_urls,
                testing::UnorderedElementsAre(url_a, extension->url()))
        << "UKM metrics should be recorded under the document URL and the "
           "extension URL.";
  }

  // 6) Now we are in A, wait for the extension to be successfully loaded.
  content::RenderFrameHostWrapper render_frame_host_a2(
      current_main_frame_host());
  ASSERT_EQ(
      kTitleModified,
      content::TitleWatcher(web_contents(), kTitleModified).WaitAndGetTitle());

  // 7) Navigate to B.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url_b));

  // 8) If `DisconnectExtensionMessagePortWhenPageEntersBFCache` is not
  // enabled, wait for A to be deleted since back/forward cache will be disabled
  // because the loaded extension is attempting to send messages to the cached
  // page A.
  if (!IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    ASSERT_TRUE(render_frame_host_a2.WaitUntilRenderFrameDeleted());
  }

  // 9) Go back to A.
  web_contents()->GetController().GoBack();
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Expect that metrics are recorded properly in `test_ukm_recorder()`, and
  // with a different source id compared to the first time.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::BackForwardCacheDisabledForRenderFrameHostReason::
          kEntryName);
  if (IsDisconnectExtensionMessagePortWhenPageEntersBFCacheEnabled()) {
    // If `DisconnectExtensionMessagePortWhenPageEntersBFCache` is enabled, the
    // page will be restored from BFCache.
    ASSERT_EQ(0u, entries.size());
  } else {
    // There should be two more new entries, one for the document URL and one
    // for the extension URL.
    EXPECT_EQ(2u + 2u, entries.size())
        << "Another 2 UKM metrics with different source ID should be recorded "
           "from the second navigation";

    std::vector<GURL> entry_urls;
    for (const ukm::mojom::UkmEntry* const entry : entries) {
      auto* src = test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      ASSERT_TRUE(src)
          << "The recorded UKM source id should have a source URL registered.";

      entry_urls.push_back(src->url());
      test_ukm_recorder()->ExpectEntryMetric(
          entry,
          ukm::builders::BackForwardCacheDisabledForRenderFrameHostReason::
              kReason2Name,
          kExtensionSentMessageToCachedFrame);
    }

    EXPECT_THAT(entry_urls,
                testing::UnorderedElementsAre(url_a, url_a, extension->url(),
                                              extension->url()))
        << "UKM metrics should be recorded under the document URL and the "
           "extension URL, and they are recorded twice each with different UKM "
           "source id.";
  }
}

class ExtensionBackForwardCacheWithPrerenderBrowserTest
    : public ExtensionBackForwardCacheBrowserTest {
 public:
  ExtensionBackForwardCacheWithPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ExtensionBackForwardCacheBrowserTest::web_contents,
            base::Unretained(this))) {}

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ExtensionBackForwardCacheBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

INSTANTIATE_TEST_SUITE_P(EventPageAndFalse,
                         ExtensionBackForwardCacheWithPrerenderBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = false,
                             .context_type = ContextType::kEventPage}));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerAndFalse,
                         ExtensionBackForwardCacheWithPrerenderBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = false,
                             .context_type = ContextType::kServiceWorker}));
INSTANTIATE_TEST_SUITE_P(EventPageAndTrue,
                         ExtensionBackForwardCacheWithPrerenderBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = true,
                             .context_type = ContextType::kEventPage}));
INSTANTIATE_TEST_SUITE_P(ServiceWorkerAndTrue,
                         ExtensionBackForwardCacheWithPrerenderBrowserTest,
                         ::testing::Values(TestParams{
                             .enable_disconnect_message_port_on_bfcache = true,
                             .context_type = ContextType::kServiceWorker}));

// Test the extension message port created during prerendering won't be closed
// after the prerendered page is activated.
IN_PROC_BROWSER_TEST_P(ExtensionBackForwardCacheWithPrerenderBrowserTest,
                       PortIsStillOpenAfterPrerenderAndActivate) {
  // This extension will automatically create a port from the content script.
  // It's only registers on title2.html, the prerendered page from this test.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("back_forward_cache")
                        .AppendASCII("content_script_auto_connect"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));

  // 1) Navigate to A.
  content::RenderFrameHostWrapper render_frame_host_a(
      ui_test_utils::NavigateToURL(browser(), url_a));

  // 2) Start a prerender.
  GURL prerender_url = embedded_test_server()->GetURL("a.com", "/title2.html");
  prerender_helper().AddPrerender(prerender_url);

  // 3) Activate.
  content::TestActivationManager activation_manager(web_contents(),
                                                    prerender_url);
  ASSERT_TRUE(
      content::ExecJs(web_contents()->GetPrimaryMainFrame(),
                      content::JsReplace("location = $1", prerender_url)));
  activation_manager.WaitForNavigationFinished();
  EXPECT_TRUE(activation_manager.was_activated());

  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      /* PrerenderFinalStatus::kActivated */ 0, 1);

  // The channel associated to the prerendered page should be open.
  EXPECT_EQ(1u, MessageService::Get(profile())->GetChannelCountForTest());
}

}  // namespace extensions
