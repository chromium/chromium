// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_result_queue.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

namespace {

// A script that can verify whether a developer-mode-restricted API is
// available. Note that we use separate verify methods here (as opposed to
// a boolean "is API available") so we can better verify expected errors and
// give more meaningful messages in the case of failure.
constexpr char kCheckApiAvailability[] =
    R"(const script =
           {
             id: 'script',
             matches: ['*://*/*'],
             js: [{file: 'script.js'}]
           };
       async function verifyApiIsAvailable() {
         let message;
         try {
           await chrome.userScripts.register([script]);
           const registered = await chrome.userScripts.getScripts();
           message =
               (registered.length == 1 &&
                registered[0].id == 'script')
                   ? 'success'
                   : 'Unexpected registration result: ' +
                         JSON.stringify(registered);
           await chrome.userScripts.unregister();
         } catch (e) {
           message = 'Unexpected error: ' + e.toString();
         }
         chrome.test.sendScriptResult(message);
       }

       async function verifyApiIsNotAvailable() {
         let message;
         try {
           // Note: we try to call a method on the API (and not just test
           // accessing it) since, if it was previously instantiated when the
           // API was available, it would still be present.
           await chrome.userScripts.register([script]);
           message = 'API unexpectedly available.';
           await chrome.userScripts.unregister();
         } catch(e) {
           const expectedError =
               `Error: Failed to read the 'userScripts' property from ` +
               `'Object': The 'userScripts' API is only available for users ` +
               'in developer mode.';
           message = e.toString() == expectedError
               ? 'success'
               : 'Unexpected error: ' + e.toString();
         }
         chrome.test.sendScriptResult(message);
       })";

}  // namespace

// And end-to-end test for extension APIs using native bindings.
class NativeBindingsApiTest : public ExtensionApiTest {
 public:
  NativeBindingsApiTest() {}

  NativeBindingsApiTest(const NativeBindingsApiTest&) = delete;
  NativeBindingsApiTest& operator=(const NativeBindingsApiTest&) = delete;

  ~NativeBindingsApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // We allowlist the extension so that it can use the cast.streaming.* APIs,
    // which are the only APIs that are prefixed twice.
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleEndToEndTest) {
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir_);
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("native_bindings/extension")) << message_;
}

// A simplistic app test for app-specific APIs.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, SimpleAppTest) {
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  ASSERT_TRUE(RunExtensionTest("native_bindings/platform_app",
                               {.launch_as_platform_app = true}))
      << message_;
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  // On reply, the extension will try to close the app window and send a
  // message.
  ExtensionTestMessageListener close_listener;
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
  ExtensionTestMessageListener listener("ready");
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
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  EXPECT_FALSE(action->GetIsVisible(tab_id));
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Navigating to example.com should show the page action.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html")));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // And the extension should be notified of the click.
  ExtensionTestMessageListener clicked_listener("clicked and removed");
  ExtensionActionAPI::Get(profile())->DispatchExtensionActionClicked(
      *action, web_contents, extension);
  ASSERT_TRUE(clicked_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, LazyListeners) {
  ProcessManager::SetEventPageIdleTimeForTesting(1);
  ProcessManager::SetEventPageSuspendingTimeForTesting(1);

  ExtensionHostTestHelper background_page_done(profile());
  background_page_done.RestrictToType(
      mojom::ViewType::kExtensionBackgroundPage);
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("native_bindings/lazy_listeners"));
  ASSERT_TRUE(extension);
  // Wait for the event page to cycle.
  background_page_done.WaitForDocumentElementAvailable();
  background_page_done.WaitForHostDestroyed();

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
  const FileSystemChooseEntryFunction::TestOptions test_options{
      .path_to_be_picked = &test_file};
  auto reset_options =
      FileSystemChooseEntryFunction::SetOptionsForTesting(test_options);
  ASSERT_TRUE(RunExtensionTest("native_bindings/instance_of",
                               {.launch_as_platform_app = true}))
      << message_;
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html")));

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
    ExtensionTestMessageListener listener("registered");
    extension = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(web_contents,
                                        GURL("https://www.example.com")));

  ExtensionTestMessageListener listener("clicked");
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "example.com", "/native_bindings/simple.html")));

  ExtensionTestMessageListener listener("callback");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that bindings are available in WebUI pages.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, WebUIBindings) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://extensions")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto api_exists = [web_contents](const std::string& api_name) {
    return content::EvalJs(web_contents,
                           base::StringPrintf("!!%s;", api_name.c_str()))
        .ExtractBool();
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

// End-to-end test for promise support on bindings for MV3 extensions, using a
// few tabs APIs. Also ensures callbacks still work for the API as expected.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, PromiseBasedAPI) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Promises",
           "manifest_version": 3,
           "version": "0.1",
           "background": {
             "service_worker": "background.js"
           },
           "permissions": ["tabs", "storage", "contentSettings", "privacy"]
         })");
  constexpr char kBackgroundJs[] =
      R"(let tabIdExample;
         let tabIdGoogle;

         chrome.test.getConfig((config) => {
           let exampleUrl = `https://example.com:${config.testServer.port}/`;
           let googleUrl = `https://google.com:${config.testServer.port}/`

           chrome.test.runTests([
             function createNewTabPromise() {
               let promise = chrome.tabs.create({url: exampleUrl});
               chrome.test.assertNoLastError();
               chrome.test.assertTrue(promise instanceof Promise);
               promise.then((tab) => {
                 let url = tab.pendingUrl;
                 chrome.test.assertEq(exampleUrl, url);
                 tabIdExample = tab.id;
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             },
             function queryTabPromise() {
               let promise = chrome.tabs.query({url: exampleUrl});
               chrome.test.assertNoLastError();
               chrome.test.assertTrue(promise instanceof Promise);
               promise.then((tabs) => {
                 chrome.test.assertTrue(tabs instanceof Array);
                 chrome.test.assertEq(1, tabs.length);
                 chrome.test.assertEq(tabIdExample, tabs[0].id);
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             },
             async function storageAreaCustomTypeWithPromises() {
               await chrome.storage.local.set({foo: 'bar', alpha: 'beta'});
               {
                 const {foo} = await chrome.storage.local.get('foo');
                 chrome.test.assertEq('bar', foo);
               }
               await chrome.storage.local.remove('foo');
               {
                 const {foo} = await chrome.storage.local.get('foo');
                 chrome.test.assertEq(undefined, foo);
               }
               let allValues = await chrome.storage.local.get(null);
               chrome.test.assertEq({alpha: 'beta'}, allValues);
               await chrome.storage.local.clear();
               allValues = await chrome.storage.local.get(null);
               chrome.test.assertEq({}, allValues);
               chrome.test.succeed();
             },
             async function contentSettingsCustomTypesWithPromises() {
               await chrome.contentSettings.cookies.set({
                   primaryPattern: '<all_urls>', setting: 'block'});
               {
                 const {setting} = await chrome.contentSettings.cookies.get({
                     primaryUrl: exampleUrl});
                 chrome.test.assertEq('block', setting);
               }
               await chrome.contentSettings.cookies.clear({});
               {
                 const {setting} = await chrome.contentSettings.cookies.get({
                     primaryUrl: exampleUrl});
                 // 'allow' is the default value for the setting.
                 chrome.test.assertEq('allow', setting);
               }
               chrome.test.succeed();
             },
             async function chromeSettingCustomTypesWithPromises() {
               // Short alias for ease of calling.
               let doNotTrack = chrome.privacy.websites.doNotTrackEnabled;
               await doNotTrack.set({value: true});
               {
                 const {value} = await doNotTrack.get({});
                 chrome.test.assertEq(true, value);
               }
               await doNotTrack.clear({});
               {
                 const {value} = await doNotTrack.get({});
                 // false is the default value for the setting.
                 chrome.test.assertEq(false, value);
               }
               chrome.test.succeed();
             },


             function createNewTabCallback() {
               chrome.tabs.create({url: googleUrl}, (tab) => {
                 let url = tab.pendingUrl;
                 chrome.test.assertEq(googleUrl, url);
                 tabIdGoogle = tab.id;
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             },
             function queryTabCallback() {
               chrome.tabs.query({url: googleUrl}, (tabs) => {
                 chrome.test.assertTrue(tabs instanceof Array);
                 chrome.test.assertEq(1, tabs.length);
                 chrome.test.assertEq(tabIdGoogle, tabs[0].id);
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             },
             function storageAreaCustomTypeWithCallbacks() {
               // Lots of stuff would probably fail if the callback version of
               // storage failed, so this is mostly just a rough sanity check.
               chrome.storage.local.set({gamma: 'delta'}, () => {
                 chrome.storage.local.get('gamma', ({gamma}) => {
                   chrome.test.assertEq('delta', gamma);
                   chrome.storage.local.clear(() => {
                     chrome.storage.local.get(null, (allValues) => {
                       chrome.test.assertEq({}, allValues);
                       chrome.test.succeed();
                     });
                   });
                 });
               });
             },
           ]);
         });)";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The above test makes 2 calls to chrome.tabs.create, so check that those
  // have been logged in the histograms we expect them to be.
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Extensions.Functions.ExtensionCalls",
                   functions::HistogramValue::TABS_CREATE));
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Extensions.Functions.ExtensionServiceWorkerCalls",
                   functions::HistogramValue::TABS_CREATE));
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Extensions.Functions.ExtensionMV3Calls",
                   functions::HistogramValue::TABS_CREATE));
}

// Tests that calling an API which supports promises using an MV2 extension does
// not get a promise based return and still needs to use callbacks when
// required.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, MV2PromisesNotSupported) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(StartEmbeddedTestServer());

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
           "name": "Promises",
           "manifest_version": 2,
           "version": "0.1",
           "background": {
             "scripts": ["background.js"]
           },
           "permissions": ["tabs", "storage", "contentSettings", "privacy"]
         })");
  constexpr char kBackgroundJs[] =
      R"(let tabIdGooge;

         chrome.test.getConfig((config) => {
           let exampleUrl = `https://example.com:${config.testServer.port}/`;
           let googleUrl = `https://google.com:${config.testServer.port}/`

           chrome.test.runTests([
             function createNewTabPromise() {
               let result = chrome.tabs.create({url: exampleUrl});
               chrome.test.assertEq(undefined, result);
               chrome.test.assertNoLastError();
               chrome.test.succeed();
             },
             function queryTabPromise() {
               let expectedError = 'Error in invocation of tabs.query(object ' +
                   'queryInfo, function callback): No matching signature.';
               chrome.test.assertThrows(chrome.tabs.query,
                                        [{url: exampleUrl}],
                                        expectedError);
               chrome.test.succeed();
             },
             function storageAreaPromise() {
               let expectedError = 'Error in invocation of storage.get(' +
                   'optional [string|array|object] keys, function callback): ' +
                   'No matching signature.';
               chrome.test.assertThrows(chrome.storage.local.get,
                                        chrome.storage.local,
                                        ['foo'], expectedError);
               chrome.test.succeed();
             },
             function contentSettingPromise() {
               let expectedError = 'Error in invocation of contentSettings' +
                   '.ContentSetting.get(object details, function callback): ' +
                   'No matching signature.';
               chrome.test.assertThrows(chrome.contentSettings.cookies.get,
                                        chrome.contentSettings.cookies,
                                        [{primaryUrl: exampleUrl}],
                                        expectedError);
               chrome.test.succeed();
             },
             function chromeSettingPromise() {
               let expectedError = 'Error in invocation of types' +
                   '.ChromeSetting.get(object details, function callback): ' +
                   'No matching signature.';
               chrome.test.assertThrows(
                   chrome.privacy.websites.doNotTrackEnabled.get,
                   chrome.privacy.websites.doNotTrackEnabled,
                   [{}],
                   expectedError);
               chrome.test.succeed();
             },
             function createNewTabCallback() {
               chrome.tabs.create({url: googleUrl}, (tab) => {
                 let url = tab.pendingUrl;
                 chrome.test.assertEq(googleUrl, url);
                 tabIdGoogle = tab.id;
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             },
             function queryTabCallback() {
               chrome.tabs.query({url: googleUrl}, (tabs) => {
                 chrome.test.assertTrue(tabs instanceof Array);
                 chrome.test.assertEq(1, tabs.length);
                 chrome.test.assertEq(tabIdGoogle, tabs[0].id);
                 chrome.test.assertNoLastError();
                 chrome.test.succeed();
               });
             }
           ]);
         });)";
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // The above test makes 2 calls to chrome.tabs.create, so check that those
  // have been logged in the histograms we expect, but not to the histograms
  // specifcally tracking service worker and MV3 calls.
  EXPECT_EQ(2, histogram_tester.GetBucketCount(
                   "Extensions.Functions.ExtensionCalls",
                   functions::HistogramValue::TABS_CREATE));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Extensions.Functions.ExtensionServiceWorkerCalls",
                   functions::HistogramValue::TABS_CREATE));
  EXPECT_EQ(0, histogram_tester.GetBucketCount(
                   "Extensions.Functions.ExtensionMV3Calls",
                   functions::HistogramValue::TABS_CREATE));
}

IN_PROC_BROWSER_TEST_F(
    NativeBindingsApiTest,
    DeveloperModeOnlyWithAPIPermissionUserIsNotInDeveloperMode) {
  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should not be available if the user is not in developer mode.
  SetCustomArg("not_in_developer_mode");
  util::SetDeveloperModeForProfile(profile(), false);
  ASSERT_TRUE(RunExtensionTest(
      "native_bindings/developer_mode_only_with_api_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    NativeBindingsApiTest,
    DeveloperModeOnlyWithAPIPermissionUserIsInDeveloperMode) {
  // With kDeveloperModeRestriction enabled, developer mode-only APIs
  // should be available if the user is in developer mode.
  SetCustomArg("in_developer_mode");
  util::SetDeveloperModeForProfile(profile(), true);
  ASSERT_TRUE(RunExtensionTest(
      "native_bindings/developer_mode_only_with_api_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    NativeBindingsApiTest,
    DeveloperModeOnlyWithoutAPIPermissionUserIsNotInDeveloperMode) {
  util::SetDeveloperModeForProfile(profile(), false);
  ASSERT_TRUE(RunExtensionTest(
      "native_bindings/developer_mode_only_without_api_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(
    NativeBindingsApiTest,
    DeveloperModeOnlyWithoutAPIPermissionUserIsInDeveloperMode) {
  util::SetDeveloperModeForProfile(profile(), true);
  ASSERT_TRUE(RunExtensionTest(
      "native_bindings/developer_mode_only_without_api_permission"))
      << message_;
}

// Tests that changing the developer mode setting affects existing renderers
// for page-based contexts (i.e., the main renderer thread).
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest,
                       SwitchingDeveloperModeAffectsExistingRenderers_Pages) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["userScripts"]
         })";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
         <html>
           <script src="page.js"></script>
         </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kCheckApiAvailability);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), "// blank");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL extension_url = extension->GetResourceURL("page.html");

  // Navigate to the extension page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));
  content::WebContents* existing_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(extension_url, existing_tab->GetLastCommittedURL());

  ScriptResultQueue result_queue;

  // By default, the API is unavailable.
  ASSERT_TRUE(content::ExecJs(existing_tab, "verifyApiIsNotAvailable();"));
  EXPECT_EQ("success", result_queue.GetNextResult());

  // Next, set the user in developer mode. Now the API should be available.
  util::SetDeveloperModeForProfile(profile(), true);
  ASSERT_TRUE(content::ExecJs(existing_tab, "verifyApiIsAvailable();"));
  EXPECT_EQ("success", result_queue.GetNextResult());

  // Toggle back to not in developer mode. The API should be unavailable again.
  util::SetDeveloperModeForProfile(profile(), false);
  ASSERT_TRUE(content::ExecJs(existing_tab, "verifyApiIsNotAvailable();"));
  EXPECT_EQ("success", result_queue.GetNextResult());
}

// Tests that incognito windows use the developer mode setting from the
// original, on-the-record profile (since incognito windows can't separately
// set developer mode).
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest,
                       IncognitoRenderersUseOriginalProfilesDevModeSetting) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "incognito": "split",
           "permissions": ["userScripts"]
         })";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
         <html>
           <script src="page.js"></script>
         </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"), kCheckApiAvailability);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), "// blank");

  const Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);

  const GURL extension_url = extension->GetResourceURL("page.html");

  Browser* incognito_browser = OpenURLOffTheRecord(profile(), extension_url);
  content::WebContents* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(incognito_tab);

  ScriptResultQueue result_queue;

  // By default, the API is unavailable.
  ASSERT_TRUE(content::ExecJs(incognito_tab, "verifyApiIsNotAvailable();"));
  EXPECT_EQ("success", result_queue.GetNextResult());

  // Next, set the user in developer mode. Now the API should be available.
  util::SetDeveloperModeForProfile(profile(), true);
  ASSERT_TRUE(content::ExecJs(incognito_tab, "verifyApiIsAvailable();"));
  EXPECT_EQ("success", result_queue.GetNextResult());

  // Toggle back to not in developer mode. The API should be unavailable again.
  util::SetDeveloperModeForProfile(profile(), false);
  ASSERT_TRUE(content::ExecJs(incognito_tab, "verifyApiIsNotAvailable();"));
  EXPECT_EQ("success", result_queue.GetNextResult());
}

// Tests that changing the developer mode setting affects existing renderers
// for service worker contexts (which run off the main thread in the renderer).
// TODO(crbug.com/40946312): Test flaky on multiple platforms
IN_PROC_BROWSER_TEST_F(
    NativeBindingsApiTest,
    DISABLED_SwitchingDeveloperModeAffectsExistingRenderers_ServiceWorkers) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["userScripts"],
           "background": {"service_worker": "background.js"}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kCheckApiAvailability);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), "// blank");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  auto call_in_service_worker = [this, extension](const std::string& script) {
    return BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  };

  auto renderer_round_trip = [this, extension]() {
    EXPECT_EQ("success",
              BackgroundScriptExecutor::ExecuteScript(
                  profile(), extension->id(),
                  "chrome.test.sendScriptResult('success');",
                  BackgroundScriptExecutor::ResultCapture::kSendScriptResult));
  };

  // By default, the API is unavailable.
  EXPECT_EQ("success", call_in_service_worker("verifyApiIsNotAvailable();"));

  // Next, set the user in developer mode. Now the API should be available.
  util::SetDeveloperModeForProfile(profile(), true);
  // We need to give the renderer time to do a few thread hops since there are
  // multiple IPC channels at play (unlike the test above). Do a round-trip to
  // the renderer to allow it to process.
  renderer_round_trip();
  EXPECT_EQ("success", call_in_service_worker("verifyApiIsAvailable();"));

  // Toggle back to not in developer mode. The API should be unavailable again.
  util::SetDeveloperModeForProfile(profile(), false);
  renderer_round_trip();
  EXPECT_EQ("success", call_in_service_worker("verifyApiIsNotAvailable();"));
}

}  // namespace extensions
