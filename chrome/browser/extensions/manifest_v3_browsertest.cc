// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
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

  ManifestV3BrowserTest(const ManifestV3BrowserTest&) = delete;
  ManifestV3BrowserTest& operator=(const ManifestV3BrowserTest&) = delete;

  ~ManifestV3BrowserTest() override {}

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool ShouldAllowMV2Extensions() override { return false; }

 private:
  ScopedCurrentChannel channel_override_{version_info::Channel::UNKNOWN};
};

IN_PROC_BROWSER_TEST_F(ManifestV3BrowserTest, ProgrammaticScriptInjection) {
  constexpr char kManifest[] =
      R"({
           "name": "Programmatic Script Injection",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" },
           "permissions": ["tabs", "scripting"],
           "host_permissions": ["*://example.com/*"]
         })";
  constexpr char kWorker[] =
      R"(chrome.tabs.onUpdated.addListener(
             async function listener(tabId, changeInfo, tab) {
           if (changeInfo.status != 'complete')
             return;
           let url = new URL(tab.url);
           if (url.hostname != 'example.com')
             return;
           // The tabs API equivalents of script injection are removed in MV3.
           chrome.test.assertEq(undefined, chrome.tabs.executeScript);
           chrome.test.assertEq(undefined, chrome.tabs.insertCSS);
           chrome.test.assertEq(undefined, chrome.tabs.removeCSS);

           chrome.tabs.onUpdated.removeListener(listener);

           function injectedFunction() {
             document.title = 'My New Title';
             return document.title;
           }
           try {
             const results = await chrome.scripting.executeScript({
               target: {tabId: tabId},
               function: injectedFunction,
             });
             chrome.test.assertTrue(!!results);
             chrome.test.assertEq(1, results.length);
             chrome.test.assertEq('My New Title', results[0].result);
             chrome.test.notifyPass();
           } catch(error) {
             chrome.test.notifyFail('executeScript promise rejected');
           }
         });
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ResultCatcher catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("example.com", "/simple.html")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  EXPECT_EQ(u"My New Title",
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
           chrome.action.setIcon({path: 'blue_icon.png'}, () => {
             chrome.test.notifyPass();
           });
         });
         chrome.test.sendMessage('ready');)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);
  test_dir.CopyFileTo(
      test_data_dir_.AppendASCII("api_test/icon_rgb_0_0_255.png"),
      FILE_PATH_LITERAL("blue_icon.png"));

  ExtensionTestMessageListener listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  std::unique_ptr<ExtensionActionTestHelper> action_test_util =
      ExtensionActionTestHelper::Create(browser());
  ASSERT_EQ(1, action_test_util->NumberOfBrowserActions());
  EXPECT_TRUE(action_test_util->HasAction(extension->id()));

  ExtensionAction* const action =
      ExtensionActionManager::Get(profile())->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_FALSE(action->HasIcon(ExtensionAction::kDefaultTabId));

  ResultCatcher catcher;
  action_test_util->Press(extension->id());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  EXPECT_TRUE(action->HasIcon(ExtensionAction::kDefaultTabId));
}

IN_PROC_BROWSER_TEST_F(ManifestV3BrowserTest, SynthesizedAction) {
  constexpr char kManifest[] =
      R"({
           "name": "Action API",
           "manifest_version": 3,
           "version": "0.1"
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ExtensionAction* const action =
      ExtensionActionManager::Get(profile())->GetExtensionAction(*extension);
  ASSERT_TRUE(action);
  EXPECT_FALSE(action->GetIsVisible(ExtensionAction::kDefaultTabId));
  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_FALSE(action->GetIsVisible(tab_id));
}

IN_PROC_BROWSER_TEST_F(ManifestV3BrowserTest,
                       DeprecatedExtensionNamespaceAPIs) {
  constexpr char kManifest[] =
      R"({
           "name": "Deprecated Extension Namespace APIs",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" }
         })";
  constexpr char kWorker[] =
      R"(chrome.test.runTests([
           function deprecatedMethods() {
             chrome.test.assertEq(undefined, chrome.extension.connect);
             chrome.test.assertEq(undefined, chrome.extension.connectNative);
             chrome.test.assertEq(undefined, chrome.extension.onConnect);
             chrome.test.assertEq(undefined,
                                  chrome.extension.onConnectExternal);
             chrome.test.assertEq(undefined, chrome.extension.onMessage);
             chrome.test.assertEq(undefined,
                                  chrome.extension.onMessageExternal);
             chrome.test.assertEq(undefined, chrome.extension.onRequest);
             chrome.test.assertEq(undefined,
                                  chrome.extension.onRequestExternal);
             chrome.test.assertEq(undefined,
                                  chrome.extension.sendNativeMessage);
             chrome.test.assertEq(undefined, chrome.extension.sendMessage);
             chrome.test.assertEq(undefined, chrome.extension.sendRequest);

             chrome.test.succeed();
           },
         ]);)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorker);

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
