// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

// And end-to-end test for extension APIs using native bindings.
class NativeBindingsApiTest : public ExtensionApiTest {
 public:
  NativeBindingsApiTest() {}
  ~NativeBindingsApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // We whitelist the extension so that it can use the cast.streaming.* APIs,
    // which are the only APIs that are prefixed twice.
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeBindingsApiTest);
};

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleEndToEndTest) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("native_bindings/extension")) << message_;
}

// A simplistic app test for app-specific APIs.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleAppTest) {
  ExtensionTestMessageListener ready_listener("ready", true);
  ASSERT_TRUE(RunPlatformAppTest("native_bindings/platform_app")) << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // On reply, the extension will try to close the app window and send a
  // message.
  ExtensionTestMessageListener close_listener(false);
  ready_listener.Reply(std::string());
  ASSERT_TRUE(close_listener.WaitUntilSatisfied());
  EXPECT_EQ("success", close_listener.message());
}

// Tests the declarativeContent API and declarative events.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, DeclarativeEvents) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an extension. On load, this extension will a) run a few simple tests
  // using chrome.test.runTests() and b) set up rules for declarative events for
  // a browser-driven test. Wait for both the tests to finish and the extension
  // to be ready.
  ExtensionTestMessageListener listener("ready", false);
  ResultCatcher catcher;
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_bindings/declarative_content"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // The extension's page action should currently be hidden.
  ExtensionAction* action =
      ExtensionActionManager::Get(profile())->GetExtensionAction(*extension);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();
  EXPECT_FALSE(action->GetIsVisible(tab_id));
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Navigating to example.com should show the page action.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html"));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // And the extension should be notified of the click.
  ExtensionTestMessageListener clicked_listener("clicked and removed", false);
  ExtensionActionAPI::Get(profile())->DispatchExtensionActionClicked(
      *action, web_contents, extension);
  ASSERT_TRUE(clicked_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, LazyListeners) {
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  LazyBackgroundObserver background_page_done;
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_bindings/lazy_listeners"));
  ASSERT_TRUE(extension);
  background_page_done.Wait();

  EventRouter* event_router = EventRouter::Get(profile());
  EXPECT_TRUE(event_router->ExtensionHasEventListener(extension->id(),
                                                      "tabs.onCreated"));
}

// End-to-end test for the fileSystem API, which includes parameters with
// instance-of requirements and a post-validation argument updater that violates
// the schema.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, FileSystemApiGetDisplayPath) {
  base::FilePath test_dir = test_data_dir_.AppendASCII("native_bindings");
  FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
      "test_root", test_dir);
  base::FilePath test_file = test_dir.AppendASCII("text.txt");
  FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
      &test_file);
  ASSERT_TRUE(RunPlatformAppTest("native_bindings/instance_of")) << message_;
}

// Tests the webRequest API, which requires IO thread requests and custom
// events.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, WebRequest) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Load an extension and wait for it to be ready.
  ResultCatcher catcher;
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("native_bindings/web_request"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html"));

  GURL expected_url = embedded_test_server()->GetURL(
      "example.com", "/native_bindings/simple2.html");
  EXPECT_EQ(expected_url, browser()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());
}

// Tests the context menu API, which includes calling sendRequest with an
// different signature than specified and using functions as properties on an
// object.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, ContextMenusTest) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Context menus",
           "manifest_version": 2,
           "version": "0.1",
           "permissions": ["contextMenus"],
           "background": {
             "scripts": ["background.js"]
           }
         })");
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      R"(chrome.contextMenus.create(
           {
             title: 'Context Menu Item',
             onclick: () => { chrome.test.sendMessage('clicked'); },
           }, () => { chrome.test.sendMessage('registered'); });)");

  const Extension* extension = nullptr;
  {
    ExtensionTestMessageListener listener("registered", false);
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(
          web_contents, GURL("https://www.example.com"), GURL(), GURL()));

  ExtensionTestMessageListener listener("clicked", false);
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  EXPECT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that unchecked errors don't impede future calls.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, ErrorsInCallbackTest) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Errors In Callback",
           "manifest_version": 2,
           "version": "0.1",
           "permissions": ["contextMenus"],
           "background": {
             "scripts": ["background.js"]
           }
         })");
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      R"(chrome.tabs.query({}, function(tabs) {
           chrome.tabs.executeScript(tabs[0].id, {code: 'x'}, function() {
             // There's an error here (we don't have permission to access the
             // host), but we don't check it so that it gets surfaced as an
             // unchecked runtime.lastError.
             // We should still be able to invoke other APIs and get correct
             // callbacks.
             chrome.tabs.query({}, function(tabs) {
               chrome.tabs.query({}, function(tabs) {
                 chrome.test.sendMessage('callback');
               });
             });
           });
         });)");

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html"));

  ExtensionTestMessageListener listener("callback", false);
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that bindings are available in WebUI pages.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, WebUIBindings) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://extensions"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto api_exists = [web_contents](const std::string& api_name) {
    bool exists = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        base::StringPrintf("window.domAutomationController.send(!!%s);",
                           api_name.c_str()),
        &exists));
    return exists;
  };

  EXPECT_TRUE(api_exists("chrome.developerPrivate"));
  EXPECT_TRUE(api_exists("chrome.developerPrivate.getProfileConfiguration"));
  EXPECT_TRUE(api_exists("chrome.management"));
  EXPECT_TRUE(api_exists("chrome.management.setEnabled"));
  EXPECT_FALSE(api_exists("chrome.networkingPrivate"));
  EXPECT_FALSE(api_exists("chrome.sockets"));
  EXPECT_FALSE(api_exists("chrome.browserAction"));
}

// Tests creating an API from a context that hasn't been initialized yet
// by doing so in a parent frame. Regression test for https://crbug.com/819968.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, APICreationFromNewContext) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("native_bindings/context_initialization"))
      << message_;
}

}  // namespace extensions
