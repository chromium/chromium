// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

class ManifestV3BrowserTest : public ExtensionBrowserTest {
 public:
  ManifestV3BrowserTest() {}
  ~ManifestV3BrowserTest() override {}

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Loads and returns an extension while ignoring warnings.
  const Extension* LoadMv3Extension(const base::FilePath& path) {
    // We ignore the manifest warnings on the extension because it includes the
    // "manifest v3 ain't quite ready yet" warning.
    // TODO(devlin): We should probably introduce a flag to specifically ignore
    // *that* warning, but no others.
    return LoadExtensionWithFlags(path, kFlagIgnoreManifestWarnings);
  }

 private:
  ScopedCurrentChannel channel_override_{version_info::Channel::UNKNOWN};

  DISALLOW_COPY_AND_ASSIGN(ManifestV3BrowserTest);
};

IN_PROC_BROWSER_TEST_F(ManifestV3BrowserTest, ProgrammaticScriptInjection) {
  constexpr char kManifest[] =
      R"({
           "name": "Programmatic Script Injection",
           "manifest_version": 3,
           "version": "0.1",
           "background": {
             "service_worker": "worker.js"
           },
           "permissions": ["tabs"],
           "host_permissions": ["*://example.com/*"]
         })";
  constexpr char kWorker[] =
      R"(chrome.tabs.onUpdated.addListener(
             function listener(tabId, changeInfo, tab) {
           if (changeInfo.status != 'complete')
             return;
           let url = new URL(tab.url);
           if (url.hostname != 'example.com')
             return;
           chrome.tabs.onUpdated.removeListener(listener);
           chrome.tabs.executeScript(
               tabId,
               {code: "document.title = 'My New Title'; document.title;"},
               (results) => {
                 chrome.test.assertNoLastError();
                 chrome.test.assertTrue(!!results);
                 chrome.test.assertEq(1, results.length);
                 chrome.test.assertEq('My New Title', results[0]);
                 chrome.test.notifyPass();
               });
         });
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ExtensionTestMessageListener listener("ready", /*will_reply=*/false);
  const Extension* extension = LoadMv3Extension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/simple.html"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  EXPECT_EQ(base::ASCIIToUTF16("My New Title"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// A simple end-to-end test exercising the new action API in Manifest V3.
// More robust tests for the action API are in extension_action_apitest.cc.
IN_PROC_BROWSER_TEST_F(ManifestV3BrowserTest, ActionAPI) {
  constexpr char kManifest[] =
      R"({
           "name": "Action API",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" },
           "action": {}
         })";
  constexpr char kWorker[] =
      R"(chrome.action.onClicked.addListener((tab) => {
           chrome.test.assertTrue(!!tab);
           chrome.test.notifyPass();
         });
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ExtensionTestMessageListener listener("ready", /*will_reply=*/false);
  const Extension* extension = LoadMv3Extension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  std::unique_ptr<BrowserActionTestUtil> action_test_util =
      BrowserActionTestUtil::Create(browser());
  ASSERT_EQ(1, action_test_util->NumberOfBrowserActions());
  EXPECT_EQ(extension->id(), action_test_util->GetExtensionId(0));

  ResultCatcher catcher;
  action_test_util->Press(0);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
