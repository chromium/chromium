// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#else
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#endif

namespace extensions {
namespace {

class TabAddedWaiter : public TabListInterfaceObserver {
 public:
  explicit TabAddedWaiter(TabListInterface* tab_list) {
    observation_.Observe(tab_list);
  }
  ~TabAddedWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override {
    run_loop_.Quit();
  }

 private:
  base::ScopedObservation<TabListInterface, TabListInterfaceObserver>
      observation_{this};
  base::RunLoop run_loop_;
};

class ViewExtensionSourceTest : public ExtensionBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_CHROMEOS)
    // These tests use chrome:// URLs and are written on the assumption devtools
    // are always available, so guarantee that assumption holds. Tests that
    // check if devtools can be disabled should use a test fixture without the
    // kForceDevToolsAvailable switch set.
    command_line->AppendSwitch(switches::kForceDevToolsAvailable);
#endif
  }

  static bool CanViewSource(content::WebContents* web_contents) {
    return web_contents->GetController().CanViewSource();
  }

  void RestoreTab() {
    sessions::TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(GetProfile());
    CHECK(service);
#if BUILDFLAG(IS_ANDROID)
    // Android does not provide BrowserWindowInterface::GetFeatures()
    // so we must get the tab context from the TabModel.
    TabListInterface* tab_list = GetTabListInterface();
    TabModel* tab_model = static_cast<TabModel*>(tab_list);
    sessions::LiveTabContext* context = tab_model->GetLiveTabContext();
#else
    sessions::LiveTabContext* context =
        GetBrowserWindowInterface()->GetFeatures().live_tab_context();
#endif
    CHECK(context);
    service->RestoreMostRecentEntry(context);
  }
};

// Verify that restoring a view-source tab for a Chrome extension works
// properly.  See https://crbug.com/41306169.
IN_PROC_BROWSER_TEST_F(ViewExtensionSourceTest, ViewSourceTabRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadExtension(
      test_data_dir_.AppendASCII("browsertest/url_rewrite/bookmarks"));

  // Go to the Chrome bookmarks URL.  It should redirect to the bookmark
  // manager Chrome extension.
  GURL bookmarks_url(chrome::kChromeUIBookmarksURL);
  content::WebContents* bookmarks_tab = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(bookmarks_tab, bookmarks_url));
  EXPECT_TRUE(CanViewSource(bookmarks_tab));
  GURL bookmarks_extension_url =
      bookmarks_tab->GetPrimaryMainFrame()->GetLastCommittedURL();
  EXPECT_TRUE(bookmarks_extension_url.SchemeIs(kExtensionScheme));

  // Open a new view-source tab for that URL.
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       bookmarks_extension_url.spec());
  ASSERT_TRUE(NavigateToURLInNewTab(view_source_url));
  content::WebContents* view_source_tab = GetActiveWebContents();
  EXPECT_EQ(view_source_url, view_source_tab->GetVisibleURL());
  EXPECT_EQ(bookmarks_extension_url,
            view_source_tab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(CanViewSource(view_source_tab));

  // Close the view-source tab.
  CloseTabForWebContents(view_source_tab);
  ASSERT_EQ(1, GetTabCount());

  // Restore the tab.  In the bug, the restored navigation was blocked, and we
  // ended up showing view-source of an about:blank page.
  TabAddedWaiter wait_for_new_tab(GetTabListInterface());
  RestoreTab();
  wait_for_new_tab.Wait();
  view_source_tab = GetActiveWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_tab));

  // Verify the browser-side URLs.  Note that without view-source, the
  // bookmarks extension visible URL would be rewritten to chrome://bookmarks,
  // but with view-source, we should still see it as
  // view-source:chrome-extension://.../.
  EXPECT_EQ(view_source_url, view_source_tab->GetVisibleURL());
  EXPECT_EQ(bookmarks_extension_url,
            view_source_tab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(CanViewSource(view_source_tab));

  // Verify that the view-source content is not empty, and that the
  // renderer-side URL is correct.
  EXPECT_GT(EvalJs(view_source_tab, "document.body.innerText.length"), 0);

  EXPECT_EQ(bookmarks_extension_url, EvalJs(view_source_tab, "location.href"));
}

}  // namespace
}  // namespace extensions
