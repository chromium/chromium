// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
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

  void OpenURLInCurrentTab(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    content::TestNavigationObserver nav_observer(web_contents);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  }

  void OpenURLInNewTab(const GURL& url) {
    content::TestNavigationObserver nav_observer(url);
    nav_observer.StartWatchingNewWebContents();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    nav_observer.Wait();
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(url, browser()
                       ->tab_strip_model()
                       ->GetActiveWebContents()
                       ->GetLastCommittedURL());
  }
};

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, MainFrameTests) {
  OpenURLInCurrentTab(embedded_test_server()->GetURL(
      "example.com", "/extensions/main_world_script_flag.html"));
  OpenURLInNewTab(
      embedded_test_server()->GetURL("chromium.org", "/title2.html"));

  ASSERT_TRUE(RunExtensionTest("scripting/main_frame", {},
                               {.ignore_manifest_warnings = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, SubFramesTests) {
  // Open up two tabs, each with cross-site iframes, one at a.com and one at
  // d.com.
  // In both cases, the cross-site iframes point to b.com and c.com.
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("a.com", "/iframe_cross_site.html"));
  OpenURLInNewTab(
      embedded_test_server()->GetURL("d.com", "/iframe_cross_site.html"));

  // From there, the test continues in the JS.
  ASSERT_TRUE(RunExtensionTest("scripting/sub_frames")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, CSSInjection) {
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("example.com", "/simple.html"));
  OpenURLInNewTab(
      embedded_test_server()->GetURL("chromium.org", "/title2.html"));
  OpenURLInNewTab(embedded_test_server()->GetURL("subframes.example",
                                                 "/iframe_cross_site.html"));

  ASSERT_TRUE(RunExtensionTest("scripting/css_injection")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, CSSRemoval) {
  ASSERT_TRUE(RunExtensionTest("scripting/remove_css")) << message_;
}

IN_PROC_BROWSER_TEST_F(ScriptingAPITest, DynamicContentScripts) {
  ASSERT_TRUE(RunExtensionTest("scripting/dynamic_scripts")) << message_;
}

// Test that if an extension with persistent scripts is quickly unloaded while
// these scripts are being fetched, requests that wait on that extension's
// script load will be unblocked. Regression for crbug.com/1250575
IN_PROC_BROWSER_TEST_F(ScriptingAPITest, RapidLoadUnload) {
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("scripting/register_one_script"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  DisableExtension(extension->id());

  // Load another extension with a content script that is injected into all
  // sites. This extension is necessary because while the register_one_script
  // extension is loading its scripts, requests which match ANY extension's
  // scripts are throttled. When an extension unloads before its script load is
  // finished and there are no more loads in progress, we want to verify that
  // ALL throttled requests are resumed, not just the ones matching the unloaded
  // extension's scripts.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("content_scripts/css_injection")));

  // First, trigger an OnLoaded event for `extension`, then quickly trigger an
  // OnUnloaded event by calling Enable/DisableExtension. Since `extension`
  // contains persistent dynamic scripts, it must fetch them from the StateStore
  // which yields control of the thread so TriggerOnUnloaded is called
  // immediately, which unloads the extension before the Statestore fetch can
  // finish.
  extension_service()->EnableExtension(extension->id());
  extension_service()->DisableExtension(extension->id(),
                                        disable_reason::DISABLE_USER_ACTION);

  // Verify that the navigation to google.com, which matches a script in the
  // css_injection extension, will complete.
  OpenURLInCurrentTab(
      embedded_test_server()->GetURL("google.com", "/simple.html"));
}

// Base test fixture for tests spanning multiple sessions where a custom arg is
// set before the test is run.
class PersistentScriptingAPITest : public ScriptingAPITest {
 public:
  PersistentScriptingAPITest() = default;

  // ScriptingAPITest override.
  void SetUpOnMainThread() override {
    ScriptingAPITest::SetUpOnMainThread();

    // Set the test name as a custom arge before the test is run. This avoids a
    // race condition where the extension loads (as part of browser startup) and
    // sends a message before a message listener in C++ has been initialized.
    SetCustomArg(testing::UnitTest::GetInstance()->current_test_info()->name());
  }

 protected:
  // Used to wait for results from extension tests. This is initialized before
  // the test is run which avoids a race condition where the extension is loaded
  // (as part of startup) and finishes its tests before the ResultCatcher is
  // created.
  ResultCatcher result_catcher_;
};

// Tests that registered content scripts which persist across sessions behave as
// expected. The test is run across three sessions.
IN_PROC_BROWSER_TEST_F(PersistentScriptingAPITest,
                       PRE_PRE_PersistentDynamicContentScripts) {
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("scripting/persistent_dynamic_scripts"));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentScriptingAPITest,
                       PRE_PersistentDynamicContentScripts) {
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

IN_PROC_BROWSER_TEST_F(PersistentScriptingAPITest,
                       PersistentDynamicContentScripts) {
  EXPECT_TRUE(result_catcher_.GetNextResult()) << result_catcher_.message();
}

}  // namespace extensions
