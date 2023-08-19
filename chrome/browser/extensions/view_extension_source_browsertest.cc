// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class ViewExtensionSourceTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

#if BUILDFLAG(IS_CHROMEOS)
    // These tests use chrome:// URLs and are written on the assumption devtools
    // are always available, so guarantee that assumption holds. Tests that
    // check if devtools can be disabled should use a test fixture without the
    // kForceDevToolsAvailable switch set.
    command_line->AppendSwitch(switches::kForceDevToolsAvailable);
#endif
  }
};

// Verify that restoring a view-source tab for a Chrome extension works
// properly.  See https://crbug.com/699428.
IN_PROC_BROWSER_TEST_F(ViewExtensionSourceTest, ViewSourceTabRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadExtension(
      test_data_dir_.AppendASCII("browsertest/url_rewrite/bookmarks"));

  // Go to the Chrome bookmarks URL.  It should redirect to the bookmark
  // manager Chrome extension.
  GURL bookmarks_url(chrome::kChromeUIBookmarksURL);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), bookmarks_url));
  EXPECT_TRUE(chrome::CanViewSource(browser()));
  content::WebContents* bookmarks_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL bookmarks_extension_url =
      bookmarks_tab->GetPrimaryMainFrame()->GetLastCommittedURL();
  EXPECT_TRUE(bookmarks_extension_url.SchemeIs(extensions::kExtensionScheme));

  // Open a new view-source tab for that URL.
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       bookmarks_extension_url.spec());
  ASSERT_TRUE(AddTabAtIndex(1, view_source_url, ui::PAGE_TRANSITION_TYPED));
  content::WebContents* view_source_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(view_source_url, view_source_tab->GetVisibleURL());
  EXPECT_EQ(bookmarks_extension_url,
            view_source_tab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(chrome::CanViewSource(browser()));

  // Close the view-source tab.
  chrome::CloseTab(browser());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Restore the tab.  In the bug, the restored navigation was blocked, and we
  // ended up showing view-source of an about:blank page.
  ui_test_utils::TabAddedWaiter wait_for_new_tab(browser());
  chrome::RestoreTab(browser());
  wait_for_new_tab.Wait();
  view_source_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(WaitForLoadStop(view_source_tab));

  // Verify the browser-side URLs.  Note that without view-source, the
  // bookmarks extension visible URL would be rewritten to chrome://bookmarks,
  // but with view-source, we should still see it as
  // view-source:chrome-extension://.../.
  EXPECT_EQ(view_source_url, view_source_tab->GetVisibleURL());
  EXPECT_EQ(bookmarks_extension_url,
            view_source_tab->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_FALSE(chrome::CanViewSource(browser()));

  // Verify that the view-source content is not empty, and that the
  // renderer-side URL is correct.
  EXPECT_GT(EvalJs(view_source_tab, "document.body.innerText.length"), 0);

  EXPECT_EQ(bookmarks_extension_url, EvalJs(view_source_tab, "location.href"));
}
