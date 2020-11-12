// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/features/feature_channel.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace extensions {

class ScriptingAPITest : public ExtensionApiTest {
 public:
  ScriptingAPITest() = default;
  ScriptingAPITest(const ScriptingAPITest&) = delete;
  ScriptingAPITest& operator=(const ScriptingAPITest&) = delete;
  ~ScriptingAPITest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  ScopedCurrentChannel current_channel_{version_info::Channel::UNKNOWN};
};

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, MainFrameTests) {
  // Start by opening up two tabs (navigating the current tab and opening a new
  // one) to example.com and chromium.org.
  {
    const GURL example_com =
        embedded_test_server()->GetURL("example.com", "/simple.html");
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    content::TestNavigationObserver nav_observer(web_contents);
    ui_test_utils::NavigateToURL(browser(), example_com);
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(example_com, web_contents->GetLastCommittedURL());
  }

  {
    const GURL chromium_org =
        embedded_test_server()->GetURL("chromium.org", "/title2.html");
    content::TestNavigationObserver nav_observer(chromium_org);
    nav_observer.StartWatchingNewWebContents();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), chromium_org, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(chromium_org, browser()
                                ->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetLastCommittedURL());
  }

  // From there, the test continues in the JS.
  ASSERT_TRUE(RunExtensionTestIgnoreManifestWarnings("scripting/main_frame"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, SubFramesTests) {
  // Open up two tabs, each with cross-site iframes, one at a.com and one at
  // d.com.
  // In both cases, the cross-site iframes point to b.com and c.com.
  {
    const GURL a_com =
        embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html");
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    content::TestNavigationObserver nav_observer(web_contents);
    ui_test_utils::NavigateToURL(browser(), a_com);
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(a_com, web_contents->GetLastCommittedURL());
  }

  {
    const GURL d_com =
        embedded_test_server()->GetURL("d.com", "/iframe_cross_site.html");
    content::TestNavigationObserver nav_observer(d_com);
    nav_observer.StartWatchingNewWebContents();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), d_com, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(d_com, browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());
  }

  // From there, the test continues in the JS.
  ASSERT_TRUE(RunExtensionTestIgnoreManifestWarnings("scripting/sub_frames"));
}

}  // namespace extensions
