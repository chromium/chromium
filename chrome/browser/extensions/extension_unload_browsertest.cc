// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"
#include "url/url_constants.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

// A helper class to wait for a particular tab count. Requires the tab list
// to outlive this object.
class TestTabListObserver : public TabListInterfaceObserver {
 public:
  explicit TestTabListObserver(TabListInterface* tab_list)
      : tab_list_(tab_list) {
    tab_list_->AddTabListInterfaceObserver(this);
  }

  TestTabListObserver(const TestTabListObserver&) = delete;
  TestTabListObserver& operator=(const TestTabListObserver&) = delete;

  ~TestTabListObserver() override {
    tab_list_->RemoveTabListInterfaceObserver(this);
  }

  void WaitForTabCount(int count) {
    if (tab_list_->GetTabCount() == count) {
      return;
    }
    desired_count_ = count;
    run_loop_.Run();
  }

 private:
  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override {
    if (tab_list_->GetTabCount() == desired_count_) {
      run_loop_.Quit();
    }
  }

  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason reason) override {
    if (tab_list_->GetTabCount() == desired_count_) {
      run_loop_.Quit();
    }
  }

  raw_ptr<TabListInterface> tab_list_;
  int desired_count_ = 0;
  base::RunLoop run_loop_;
};

}  // namespace

class ExtensionUnloadBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("maps.google.com", "127.0.0.1");
  }

  TabListInterface* GetTabListInterface() {
    auto* tab_list = TabListInterface::From(browser_window_interface());
    CHECK(tab_list);
    return tab_list;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionUnloadBrowserTest, TestUnload) {
  // Load an extension that installs unload and beforeunload listeners.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("unload_listener"));
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  auto* tab_list = GetTabListInterface();
  ASSERT_EQ(1, tab_list->GetTabCount());
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), GURL("chrome://version/")));
  GURL initial_tab_url =
      tab_list->GetTab(0)->GetContents()->GetLastCommittedURL();
  NavigateToURLInNewTab(extension->GetResourceURL("page.html"));
  EXPECT_EQ(2, tab_list->GetTabCount());
  DisableExtension(id);
  // There should only be one remaining web contents - the initial one.
  ASSERT_EQ(1, tab_list->GetTabCount());
  EXPECT_EQ(initial_tab_url,
            tab_list->GetTab(0)->GetContents()->GetLastCommittedURL());
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
  auto* tab_list = GetTabListInterface();
  ASSERT_EQ(1, tab_list->GetTabCount());
  GURL test_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), test_url));

  // The content script sends an XHR with the webpage's (rather than
  // extension's) Origin header - this should succeed (given that
  // xhr.txt.mock-http-headers says `Access-Control-Allow-Origin: *`).
  const char kSendXhrScript[] = "document.getElementById('xhrButton').click();";
  content::DOMMessageQueue message_queue;
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), kSendXhrScript));
  std::string ack;
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);

  DisableExtension(id);

  // The tab should still be open with the content script injected.
  ASSERT_EQ(1, tab_list->GetTabCount());
  EXPECT_EQ(test_url,
            tab_list->GetTab(0)->GetContents()->GetLastCommittedURL());

  // The content script sends an XHR with the webpage's (rather than
  // extension's) Origin header - this should succeed (given that
  // xhr.txt.mock-http-headers says `Access-Control-Allow-Origin: *`).
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), kSendXhrScript));
  EXPECT_TRUE(message_queue.WaitForMessage(&ack));
  EXPECT_EQ("true", ack);

  // Ensure the process has not been killed.
  EXPECT_TRUE(
      GetActiveWebContents()->GetPrimaryMainFrame()->IsRenderFrameLive());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests that windows with opaque origins opened by the extension are closed
// when the extension is unloaded. Regression test for
// https://crbug.com/40092671.
// TODO(crbug.com/483480455): Port to desktop Android.
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

  auto* tab_list = GetTabListInterface();
  EXPECT_EQ(2, tab_list->GetTabCount());
  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_EQ(about_blank, web_contents->GetLastCommittedURL());
  url::Origin frame_origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  url::SchemeHostPort precursor_tuple =
      frame_origin.GetTupleOrPrecursorTupleIfOpaque();
  EXPECT_EQ(kExtensionScheme, precursor_tuple.scheme());
  EXPECT_EQ(extension->id(), precursor_tuple.host());

  TestTabListObserver test_tab_strip_model_observer(tab_list);
  extension_registrar()->DisableExtension(
      extension->id(), {disable_reason::DISABLE_USER_ACTION});
  test_tab_strip_model_observer.WaitForTabCount(1);

  EXPECT_EQ(1, tab_list->GetTabCount());
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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
  NavigateToURLInNewTab(page_url);

  auto* tab_list = GetTabListInterface();
  EXPECT_EQ(2, tab_list->GetTabCount());

  content::WebContents* active_tab = GetActiveWebContents();
  EXPECT_EQ(page_url, active_tab->GetLastCommittedURL());

  {
    content::ScopedAllowRendererCrashes allow_renderer_crashes(
        active_tab->GetPrimaryMainFrame()->GetProcess());
    // Ignore the return value for navigation, since it will crash.
    (void)NavigateToURL(active_tab, GURL("chrome://crash"));
  }

  // There should still be two open tabs, but the active one is crashed.
  EXPECT_EQ(2, tab_list->GetTabCount());
  EXPECT_TRUE(active_tab->IsCrashed());

  // Even though the tab is crashed, it should still have the last committed
  // URL of the extension page.
  EXPECT_EQ(page_url, active_tab->GetLastCommittedURL());

  // Unloading the extension should close the crashed tab, since its origin was
  // still the extension's origin.
  TestTabListObserver test_tab_list_observer(tab_list);
  extension_registrar()->DisableExtension(
      extension->id(), {disable_reason::DISABLE_USER_ACTION});
  test_tab_list_observer.WaitForTabCount(1);

  EXPECT_EQ(1, tab_list->GetTabCount());
  EXPECT_NE(
      extension->url().DeprecatedGetOriginAsURL(),
      GetActiveWebContents()->GetLastCommittedURL().DeprecatedGetOriginAsURL());
}

// TODO(devlin): Investigate what to do for embedded iframes.

}  // namespace extensions
