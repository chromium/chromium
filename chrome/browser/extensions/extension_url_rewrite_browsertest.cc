// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/search/ntp_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

using content::NavigationEntry;

class ExtensionURLRewriteBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  void SetUp() override {
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
    extensions::ExtensionBrowserTest::SetUp();
  }

 protected:
  std::string GetLocationBarText() const {
    return base::UTF16ToUTF8(
        browser()->window()->GetLocationBar()->GetOmniboxView()->GetText());
  }

  GURL GetLocationBarTextAsURL() const {
    return GURL(GetLocationBarText());
  }

  content::NavigationController* GetNavigationController() const {
    return &browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetController();
  }

  NavigationEntry* GetNavigationEntry() const {
    return GetNavigationController()->GetVisibleEntry();
  }

  base::FilePath GetTestExtensionPath(const char* extension_name) const {
    return test_data_dir_.AppendASCII("browsertest/url_rewrite/").
        AppendASCII(extension_name);
  }

  // Navigates to |url| and tests that the location bar and the |virtual_url|
  // correspond to |url|, while the real URL of the navigation entry uses the
  // chrome-extension:// scheme.
  void TestExtensionURLOverride(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(url, GetLocationBarTextAsURL());
    EXPECT_EQ(url, GetNavigationEntry()->GetVirtualURL());
    EXPECT_TRUE(
        GetNavigationEntry()->GetURL().SchemeIs(extensions::kExtensionScheme));
  }

  // Navigates to |url| and tests that the location bar is empty while the
  // |virtual_url| is the same as |url|.
  void TestURLNotShown(const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ("", GetLocationBarText());
    EXPECT_EQ(url, GetNavigationEntry()->GetVirtualURL());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, NewTabPageURL) {
  // Navigate to chrome://newtab and check that the location bar text is blank.
  // We do not use TestURLNotShown here because the virtual URL may be
  // updated to the local NTP since we do not have a network connection to
  // reach the remote NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  EXPECT_EQ("", GetLocationBarText());
  // Check that the actual and virtual URL corresponds to the new tab URL.
  EXPECT_EQ(ntp_test_utils::GetFinalNtpUrl(browser()->profile()),
            GetNavigationEntry()->GetVirtualURL());
  EXPECT_TRUE(
      search::IsNTPOrRelatedURL(GetNavigationEntry()->GetURL(), profile()));
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, NewTabPageURLOverride) {
  // Load an extension to override the NTP and check that the location bar text
  // is blank after navigating to chrome://newtab.
  ASSERT_TRUE(LoadExtension(GetTestExtensionPath("newtab")));
  TestURLNotShown(GURL(chrome::kChromeUINewTabURL));
  // Check that the internal URL uses the chrome-extension:// scheme.
  EXPECT_TRUE(GetNavigationEntry()->GetURL().SchemeIs(
      extensions::kExtensionScheme));
}

IN_PROC_BROWSER_TEST_F(ExtensionURLRewriteBrowserTest, BookmarksURLOverride) {
  // Load an extension that overrides chrome://bookmarks.
  ASSERT_TRUE(LoadExtension(GetTestExtensionPath("bookmarks")));
  // Navigate to chrome://bookmarks and check that the location bar URL is what
  // was entered and the internal URL uses the chrome-extension:// scheme.
  TestExtensionURLOverride(GURL(chrome::kChromeUIBookmarksURL));
}
