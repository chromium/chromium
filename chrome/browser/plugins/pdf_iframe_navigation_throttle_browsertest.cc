// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pdf_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

using content::JsReplace;
using content::RenderFrameHost;
using content::TestNavigationManager;
using content::WebContents;
using content::test::PrerenderHostObserver;
using content::test::PrerenderHostRegistryObserver;

// We'll use this to block the PDF plugin from loading to force the HTML
// fallback in the NavigationThrottle.
class BlockAllPluginServiceFilter : public content::PluginServiceFilter {
 public:
  bool IsPluginAvailable(content::BrowserContext* browser_context,
                         const content::WebPluginInfo& plugin) override {
    return false;
  }

  bool CanLoadPlugin(int render_process_id,
                     const base::FilePath& path) override {
    return false;
  }
};

class PDFIFrameNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  PDFIFrameNavigationThrottleBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PDFIFrameNavigationThrottleBrowserTest::web_contents,
            base::Unretained(this))) {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->Init();
    old_plugin_service_filter_ = plugin_service->GetFilter();
    plugin_service->SetFilter(&block_all_plugins_);
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    content::PluginService::GetInstance()->SetFilter(
        old_plugin_service_filter_);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::test::PrerenderTestHelper prerender_helper_;
  raw_ptr<content::PluginServiceFilter> old_plugin_service_filter_;
  BlockAllPluginServiceFilter block_all_plugins_;
};

// TODO(crbug.com/40180674): The PDF viewer cannot currently be prerendered
// correctly. Once this is supported, this test should be re-enabled. This test
// checks that the throttle is able to navigate the iframe'd PDF to the fallback
// HTML content even while it is prerendering.
IN_PROC_BROWSER_TEST_F(PDFIFrameNavigationThrottleBrowserTest,
                       DISABLED_HTMLFallbackInPrerender) {
  const GURL kUrl(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));

  const GURL kPrerenderUrl =
      embedded_test_server()->GetURL("/pdf/test-iframe.html");

  const GURL kPdfUrl =
      embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf");

  const std::string html = GetPDFPlaceholderHTML(kPdfUrl);
  const GURL kFallbackPdfUrl("data:text/html," + base::EscapePath(html));
  TestNavigationManager pdf_navigation(web_contents(), kFallbackPdfUrl);

  // Trigger a prerender of a page containing an iframe with a pdf file.
  {
    PrerenderHostRegistryObserver registry_observer(*web_contents());
    prerender_helper_.AddPrerenderAsync(kPrerenderUrl);
    registry_observer.WaitForTrigger(kPrerenderUrl);
  }

  // The PDFIFrameNavigationThrottle will cancel the navigation to the pdf file
  // and navigate the frame anew to a fallback data: URL. Since its in a
  // prerendering frame tree, and data: URLs are cross-origin with the main
  // frame, we expect the navigation to be deferred during WillStartRequest
  // until the prerender is activated.
  {
    ASSERT_TRUE(pdf_navigation.WaitForFirstYieldAfterDidStartNavigation());
    EXPECT_FALSE(pdf_navigation.GetNavigationHandle()->HasCommitted());
    EXPECT_TRUE(pdf_navigation.GetNavigationHandle()->IsDeferredForTesting());
  }

  // Now navigate the primary page to the prerendered URL so that we activate
  // the prerender.
  {
    PrerenderHostObserver prerender_observer(*web_contents(), kPrerenderUrl);
    ASSERT_TRUE(ExecJs(web_contents()->GetPrimaryMainFrame(),
                       JsReplace("location = $1", kPrerenderUrl)));
    prerender_observer.WaitForActivation();
  }

  // Now that we're activated, the fallback navigation should be able to
  // finish. The initial PDF navigation should be cancelled by the throttle and
  // fallback content loaded in its place.
  {
    ASSERT_TRUE(pdf_navigation.WaitForNavigationFinished());
    EXPECT_TRUE(pdf_navigation.was_committed());
    EXPECT_TRUE(pdf_navigation.was_successful());

    content::RenderFrameHost* child_frame =
        ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
    ASSERT_TRUE(child_frame);
    EXPECT_EQ(child_frame->GetLastCommittedURL(), kFallbackPdfUrl);
  }
}
