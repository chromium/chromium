// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace extensions {

namespace {

// A helper class to wait for a particular tab count. Requires the tab strip
// to outlive this object.
class TestTabStripModelObserver : public TabStripModelObserver {
 public:
  explicit TestTabStripModelObserver(TabStripModel* model)
      : model_(model), desired_count_(0) {
    model->AddObserver(this);
  }

  TestTabStripModelObserver(const TestTabStripModelObserver&) = delete;
  TestTabStripModelObserver& operator=(const TestTabStripModelObserver&) =
      delete;

  ~TestTabStripModelObserver() override = default;

  void WaitForTabCount(int count) {
    if (model_->count() == count)
      return;
    desired_count_ = count;
    run_loop_.Run();
  }

 private:
  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (model_->count() == desired_count_)
      run_loop_.Quit();
  }

  raw_ptr<TabStripModel> model_;
  int desired_count_;
  base::RunLoop run_loop_;
};

}  // namespace

class ExtensionUnloadBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("maps.google.com", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionUnloadBrowserTest, TestUnload) {
  // Load an extension that installs unload and beforeunload listeners.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("unload_listener"));
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  GURL initial_tab_url =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetLastCommittedURL();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  DisableExtension(id);
  // There should only be one remaining web contents - the initial one.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      initial_tab_url,
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetLastCommittedURL());
}

// After an extension is uninstalled, network requests from its content scripts
// should fail but not kill the renderer process.
IN_PROC_BROWSER_TEST_F(ExtensionUnloadBrowserTest, UnloadWithContentScripts) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load an extension with a content script that has a button to send XHRs.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("xhr_from_content_script"));
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // The content script sends an XHR with the webpage's (rather than
  // extension's) Origin header - this should succeed (given that
  // xhr.txt.mock-http-headers says `Access-Control-Allow-Origin: *`).
  const char kSendXhrScript[] = "document.getElementById('xhrButton').click();";
  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(), kSendXhrScript));
  std::string ack;
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);

  DisableExtension(id);

  // The tab should still be open with the content script injected.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      test_url,
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetLastCommittedURL());

  // The content script sends an XHR with the webpage's (rather than
  // extension's) Origin header - this should succeed (given that
  // xhr.txt.mock-http-headers says `Access-Control-Allow-Origin: *`).
  EXPECT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetActiveWebContents(), kSendXhrScript));
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);

  // Ensure the process has not been killed.
  EXPECT_TRUE(browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetPrimaryMainFrame()
                  ->IsRenderFrameLive());
}

// Tests that windows with opaque origins opened by the extension are closed
// when the extension is unloaded. Regression test for https://crbug.com/894477.
IN_PROC_BROWSER_TEST_F(ExtensionUnloadBrowserTest, OpenedOpaqueWindows) {
  TestExtensionDir test_dir;
  constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 2,
           "version": "0.1",
           "background": {
             "scripts": ["background.js"]
           }
         })";
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     "window.open('about:blank');");

  const GURL about_blank(url::kAboutBlankURL);
  content::TestNavigationObserver about_blank_observer(about_blank);
  about_blank_observer.StartWatchingNewWebContents();
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  about_blank_observer.WaitForNavigationFinished();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(about_blank, web_contents->GetLastCommittedURL());
  url::Origin frame_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  url::SchemeHostPort precursor_tuple =
      frame_origin.GetTupleOrPrecursorTupleIfOpaque();
  EXPECT_EQ(kExtensionScheme, precursor_tuple.scheme());
  EXPECT_EQ(extension->id(), precursor_tuple.host());

  TestTabStripModelObserver test_tab_strip_model_observer(
      browser()->tab_strip_model());
  extension_service()->DisableExtension(extension->id(),
                                        disable_reason::DISABLE_USER_ACTION);
  test_tab_strip_model_observer.WaitForTabCount(1);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(ExtensionUnloadBrowserTest, CrashedTabs) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "test extension",
           "manifest_version": 2,
           "version": "0.1"
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"),
                     "<!doctype html><html><body>Hello world</body></html>");
  scoped_refptr<const Extension> extension(
      LoadExtension(test_dir.UnpackedPath()));
  ASSERT_TRUE(extension);
  const GURL page_url = extension->GetResourceURL("page.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(page_url, active_tab->GetLastCommittedURL());

  {
    content::ScopedAllowRendererCrashes allow_renderer_crashes(
        active_tab->GetPrimaryMainFrame()->GetProcess());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("chrome://crash"), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  // There should still be two open tabs, but the active one is crashed.
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(active_tab->IsCrashed());

  // Even though the tab is crashed, it should still have the last committed
  // URL of the extension page.
  EXPECT_EQ(page_url, active_tab->GetLastCommittedURL());

  // Unloading the extension should close the crashed tab, since its origin was
  // still the extension's origin.
  TestTabStripModelObserver test_tab_strip_model_observer(
      browser()->tab_strip_model());
  extension_service()->DisableExtension(extension->id(),
                                        disable_reason::DISABLE_USER_ACTION);
  test_tab_strip_model_observer.WaitForTabCount(1);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(extension->url().DeprecatedGetOriginAsURL(),
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL()
                .DeprecatedGetOriginAsURL());
}

// TODO(devlin): Investigate what to do for embedded iframes.

}  // namespace extensions
