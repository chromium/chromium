// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;
using extensions::ResultCatcher;

class IncognitoApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(IncognitoApiTest, IncognitoNoScript) {
  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("incognito")
      .AppendASCII("content_scripts")));

  // Open incognito window and navigate to test page.
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();

  // Verify the script didn't run.
  EXPECT_EQ(true, content::EvalJs(tab, "document.title == 'Unmodified'"));
}

IN_PROC_BROWSER_TEST_F(IncognitoApiTest, IncognitoYesScript) {
  // Load a dummy extension. This just tests that we don't regress a
  // crash fix when multiple incognito- and non-incognito-enabled extensions
  // are mixed.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("content_scripts")
      .AppendASCII("all_frames")));

  // Loads a simple extension which attempts to change the title of every page
  // that loads to "modified".
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("content_scripts"),
      {.allow_in_incognito = true}));

  // Dummy extension #2.
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("content_scripts").AppendASCII("isolated_world1")));

  // Open incognito window and navigate to test page.
  Browser* otr_browser = OpenURLOffTheRecord(
      browser()->profile(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();

  // Verify the script ran.
  EXPECT_EQ(true, content::EvalJs(tab, "document.title == 'modified'"));
}

// Tests that an extension which is enabled for incognito mode doesn't
// accidentally create an incognito profile.
IN_PROC_BROWSER_TEST_F(IncognitoApiTest, DontCreateIncognitoProfile) {
  ASSERT_FALSE(browser()->profile()->HasPrimaryOTRProfile());
  ASSERT_TRUE(RunExtensionTest("incognito/dont_create_profile", {},
                               {.allow_in_incognito = true}))
      << message_;
  ASSERT_FALSE(browser()->profile()->HasPrimaryOTRProfile());
}

IN_PROC_BROWSER_TEST_F(IncognitoApiTest, Incognito) {
  ResultCatcher catcher;

  // Open incognito window and navigate to test page.
  OpenURLOffTheRecord(
      browser()->profile(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("incognito").AppendASCII("apis"),
                    {.allow_in_incognito = true}));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that the APIs in an incognito-enabled split-mode extension work
// properly.
IN_PROC_BROWSER_TEST_F(IncognitoApiTest, IncognitoSplitMode) {
  // We need 2 ResultCatchers because we'll be running the same test in both
  // regular and incognito mode.
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());
  ResultCatcher catcher_incognito;
  catcher_incognito.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  ExtensionTestMessageListener listener("waiting", ReplyBehavior::kWillReply);
  ExtensionTestMessageListener listener_incognito("waiting_incognito",
                                                  ReplyBehavior::kWillReply);

  // Open incognito window and navigate to test page.
  OpenURLOffTheRecord(browser()->profile(), embedded_test_server()->GetURL(
                                                "/extensions/test_file.html"));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("split"),
      {.allow_in_incognito = true}));

  // Wait for both extensions to be ready before telling them to proceed.
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_TRUE(listener_incognito.WaitUntilSatisfied());
  listener.Reply("go");
  listener_incognito.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher_incognito.GetNextResult()) << catcher.message();
}

// Tests that the APIs in an incognito-disabled extension don't see incognito
// events or callbacks.
IN_PROC_BROWSER_TEST_F(IncognitoApiTest, IncognitoDisabled) {
  ResultCatcher catcher;
  ExtensionTestMessageListener listener("createIncognitoTab",
                                        ReplyBehavior::kWillReply);

  // Open incognito window and navigate to test page.
  OpenURLOffTheRecord(browser()->profile(), embedded_test_server()->GetURL(
                                                "/extensions/test_file.html"));

  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("incognito").AppendASCII("apis_disabled")));

  EXPECT_TRUE(listener.WaitUntilSatisfied());
  OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  listener.Reply("created");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Test that opening a popup from an incognito browser window works properly.
// http://crbug.com/180759.
IN_PROC_BROWSER_TEST_F(IncognitoApiTest, DISABLED_IncognitoPopup) {
  ResultCatcher catcher;

  const extensions::Extension* const extension = LoadExtension(
      test_data_dir_.AppendASCII("incognito").AppendASCII("popup"),
      {.allow_in_incognito = true});
  ASSERT_TRUE(extension);

  // Open incognito window and navigate to test page.
  Browser* incognito_browser = OpenURLOffTheRecord(
      browser()->profile(),
      embedded_test_server()->GetURL("/extensions/test_file.html"));

  // Simulate the incognito's browser action being clicked.
  ExtensionActionTestHelper::Create(incognito_browser)->Press(extension->id());

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
