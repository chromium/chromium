// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/browser/browser_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

namespace apps {

namespace {

class AppsBrowserApiTest : public extensions::ExtensionApiTest {
 public:
  AppsBrowserApiTest() = default;
  AppsBrowserApiTest(const AppsBrowserApiTest&) = delete;
  AppsBrowserApiTest& operator=(const AppsBrowserApiTest&) = delete;
  ~AppsBrowserApiTest() override = default;

  // extensions::ExtensionApiTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AppsBrowserApiTest, OpenTab) {
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Browser API test",
           "version": "2.0",
           "manifest_version": 2,
           "permissions": ["browser"],
           "app": {
             "background": {
               "scripts": ["background.js"]
             }
           }
         })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     R"(chrome.test.getConfig((config) => {
           const url =
               `http://example.com:${config.testServer.port}/simple.html`;
           chrome.test.runTests([
             function() {
               chrome.test.assertTrue(!!chrome);
               chrome.test.assertTrue(!!chrome.browser);
               chrome.test.assertTrue(!!chrome.browser.openTab);
               chrome.browser.openTab({url: url}, function() {
                  chrome.test.assertNoLastError();
                  chrome.test.succeed();
               });
             }
           ]);
         });)");

  extensions::ResultCatcher result_catcher;
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();

  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_EQ(embedded_test_server()->GetURL("example.com", "/simple.html"),
            web_contents->GetLastCommittedURL());
}

// Tests calling browser.openTab() when the only active browser is an incognito
// browser.
// Regression test for https://crbug.com/490330704.
IN_PROC_BROWSER_TEST_F(AppsBrowserApiTest,
                       OpenTabWithOnlyIncognitoBrowserOpen) {
  static constexpr char kManifest[] =
      R"({
           "name": "Browser API test incognito",
           "version": "2.0",
           "manifest_version": 2,
           "permissions": ["browser"],
           "app": {
             "background": {
               "scripts": ["background.js"]
             }
           }
         })";

  static constexpr char kBackgroundJs[] =
      R"(chrome.test.getConfig((config) => {
           const url =
               `http://example.com:${config.testServer.port}/simple.html`;
           chrome.test.sendMessage('ready', function(reply) {
             chrome.browser.openTab({url: url}, function() {
                chrome.test.assertNoLastError();
                chrome.test.succeed();
             });
           });
         });)";
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);

  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  extensions::ResultCatcher result_catcher;

  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Create an incognito browser window.
  Browser* incognito_browser = CreateIncognitoBrowser(profile());
  ASSERT_TRUE(incognito_browser);

  // Close the main browser window. This leaves the incognito window as the
  // only open browser.
  CloseBrowserSynchronously(browser());

  // Tell the extension to continue and call openTab.
  listener.Reply("");

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace apps
