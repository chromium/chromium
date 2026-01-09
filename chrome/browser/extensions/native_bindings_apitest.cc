// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "extensions/common/extension_features.h"
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
    R"(
       async function verifyApiIsAvailable() {
         let message;
         try {
           const tabs = await chrome.tabs.query({});
           chrome.test.assertEq(1, tabs.length);
           const debuggee = {tabId: tabs[0].id};
           await chrome.debugger.attach(debuggee, '1.3');
           await chrome.debugger.detach(debuggee);
           message = 'success';
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
           await chrome.debugger.getTargets();
           message = 'API unexpectedly available.';
         } catch(e) {
           const expectedError =
               `Error: Failed to read the 'debugger' property from ` +
               `'Object': The 'debugger' API is only available for users ` +
               'in developer mode.';
           message = e.toString() == expectedError
               ? 'success'
               : 'Unexpected error: ' + e.toString();
         }
         chrome.test.sendScriptResult(message);
       })";

constexpr char kCheckApiAvailabilityUserScripts[] =
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

bool ApiExists(content::WebContents* web_contents,
               const std::string& api_name) {
  return content::EvalJs(web_contents,
                         base::StringPrintf("!!%s;", api_name.c_str()))
      .ExtractBool();
}

bool ObjectIsDefined(content::WebContents* web_contents,
                     const std::string& object_name) {
  return content::EvalJs(web_contents,
                         base::StringPrintf("self.hasOwnProperty('%s');",
                                            object_name.c_str()))
      .ExtractBool();
}

}  // namespace

// And end-to-end test for extension APIs using native bindings.
class NativeBindingsApiTest : public ExtensionApiTest {
 public:
  NativeBindingsApiTest() = default;

  NativeBindingsApiTest(const NativeBindingsApiTest&) = delete;
  NativeBindingsApiTest& operator=(const NativeBindingsApiTest&) = delete;

  ~NativeBindingsApiTest() override = default;

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
  content::WebContents* web_contents = GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  EXPECT_FALSE(action->GetIsVisible(tab_id));
  EXPECT_TRUE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // Navigating to example.com should show the page action.
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL(
                        "example.com", "/native_bindings/simple.html")));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(action->GetIsVisible(tab_id));
  EXPECT_FALSE(action->GetDeclarativeIcon(tab_id).IsEmpty());

  // And the extension should be notified of the click.
  ExtensionTestMessageListener clicked_listener("clicked and removed");
  ExtensionActionDispatcher::Get(profile())->DispatchExtensionActionClicked(
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

  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(
      web_contents, embedded_test_server()->GetURL(
                        "example.com", "/native_bindings/simple.html")));

  GURL expected_url = embedded_test_server()->GetURL(
      "example.com", "/native_bindings/simple2.html");
  EXPECT_EQ(expected_url, web_contents->GetLastCommittedURL());
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

  content::WebContents* web_contents = GetActiveWebContents();
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

  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    embedded_test_server()->GetURL(
                        "example.com", "/native_bindings/simple.html")));

  ExtensionTestMessageListener listener("callback");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that bindings are available in WebUI pages.
IN_PROC_BROWSER_TEST_F(NativeBindingsApiTest, WebUIBindings) {
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://extensions")));

  EXPECT_TRUE(ApiExists(web_contents, "chrome.developerPrivate"));
  EXPECT_TRUE(ApiExists(web_contents,
                        "chrome.developerPrivate.getProfileConfiguration"));
  EXPECT_TRUE(ApiExists(web_contents, "chrome.management"));
  EXPECT_TRUE(ApiExists(web_contents, "chrome.management.setEnabled"));
  EXPECT_FALSE(ApiExists(web_contents, "chrome.networkingPrivate"));
  EXPECT_FALSE(ApiExists(web_contents, "chrome.sockets"));
  EXPECT_FALSE(ApiExists(web_contents, "chrome.browserAction"));
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
  // specifically tracking service worker and MV3 calls.
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

class NativeBindingsBrowserNamespaceTest : public NativeBindingsApiTest {
 public:
  NativeBindingsBrowserNamespaceTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kExtensionBrowserNamespaceAlternative);
  }

  NativeBindingsBrowserNamespaceTest(
      const NativeBindingsBrowserNamespaceTest&) = delete;
  const NativeBindingsBrowserNamespaceTest& operator=(
      const NativeBindingsBrowserNamespaceTest&) = delete;
  ~NativeBindingsBrowserNamespaceTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that extension background script contexts have access to
// `chrome.<extension_api>` and `browser.<extension_api>` objects.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_ExtensionBackground) {
  ASSERT_TRUE(RunExtensionTest("browser_object/background_context"))
      << message_;
}

// Tests that extension foreground script contexts (e.g. content script,
// extension page) have access to `chrome.<extension_api>` and
// `browser.<extension_api>` objects.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_ExtensionForeground) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL& test_website =
      embedded_test_server()->GetURL("a.com", "/title1.html");

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("browser_object/foreground_context"));
  ASSERT_TRUE(extension);

  // Content script.
  ResultCatcher catcher;
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, test_website));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  // Extension page.
  ResultCatcher extension_resource_catcher;
  ASSERT_TRUE(NavigateToURL(
      web_contents,
      GURL(extension->GetResourceURL("extension_resource_page.html"))));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that an externally connectable webpage has access to
// `chrome.<extension_api>` and `browser.<extension_api>` objects. Additionally
// it tests that `chrome.app` is bound, but `browser.app` is not.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_ExternallyConnectableWebpage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL& test_website =
      embedded_test_server()->GetURL("a.com", "/title1.html");

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
          "name": "Externally connectable test extension",
          "version": "0.1",
          "manifest_version": 3,
          "externally_connectable": {
            "matches": ["*://a.com/*"]
          }
        })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "");
  ASSERT_TRUE(LoadExtension(test_dir.UnpackedPath()));

  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, test_website));

  EXPECT_TRUE(ApiExists(web_contents, "chrome.runtime"));
  EXPECT_TRUE(ApiExists(web_contents, "browser.runtime"));
  EXPECT_TRUE(ApiExists(web_contents, "chrome.app"));
  EXPECT_FALSE(ApiExists(web_contents, "browser.app"));
}

// Tests that the `browser` namespace is not available in WebUI.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest, WebUIBindings) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://extensions")));

  EXPECT_TRUE(ObjectIsDefined(web_contents, "chrome"));
  EXPECT_FALSE(ObjectIsDefined(web_contents, "browser"));
}

// Tests that an arbitrary web page with no extension API access has access to
// the `chrome` namespace, but not the browser namespace.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       TestNonExtensionChromeAndBrowserObjects) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const GURL& test_website =
      embedded_test_server()->GetURL("a.com", "/title1.html");
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents, test_website));

  EXPECT_TRUE(ObjectIsDefined(web_contents, "chrome"));
  EXPECT_FALSE(ObjectIsDefined(web_contents, "browser"));
}

// Tests that the browser namespace includes the devtools API.
// Regression test for https://crbug.com/470092691.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_DevTools) {
  // Load an extension that creates a devtools page/panel.
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
          "name": "DevTools test extension",
          "version": "0.1",
          "manifest_version": 3,
          "devtools_page": "devtools.html"
        })");
  test_dir.WriteFile(FILE_PATH_LITERAL("devtools.html"),
                     "<script src='devtools.js'></script>");
  // Tests that the extension devtools page can access the devtools API.
  test_dir.WriteFile(FILE_PATH_LITERAL("devtools.js"),
                     R"(chrome.test.runTests([
                        function checkDevTools() {
                          chrome.test.assertTrue(chrome.devtools !== undefined,
                                                 'chrome.devtools');
                          chrome.test.assertTrue(browser.devtools !== undefined,
                                                 'browser.devtools');
                          chrome.test.succeed();
                        }
                      ]);)");

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Open a devtools window to get the extension devtools page (and tests) to
  // load.
  DevToolsWindow::OpenDevToolsWindow(GetActiveWebContents(),
                                     DevToolsToggleAction::Show(),
                                     DevToolsOpenedByAction::kUnknown);

  // Wait for the devtools tests to run.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that standard APIs like `runtime` are distinct objects in the `chrome`
// and `browser` namespaces, even if they point to the same underlying API.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_ApiAliasing) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
          "name": "Api Aliasing test",
          "version": "0.1",
          "manifest_version": 3,
          "background": {"service_worker": "background.js"}
        })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     R"(chrome.test.runTests([
                        function checkApiAliasing() {
                          chrome.test.assertTrue(chrome.runtime !== undefined,
                                                 'chrome.runtime');
                          // Standard APIs like `runtime` are independently
                          // created for both `chrome` and `browser` namespaces.
                          // They are not aliases of each other identity-wise,
                          // but they provide the same functionality.
                          chrome.test.assertEq(chrome.runtime, browser.runtime);

                          // Try to modify chrome.runtime as representative of
                          // most APIs since they use the same bindings
                          // accessor. In a non-extension-devtools context like
                          // this, the API root on `chrome` is typically
                          // writable.
                          let originalRuntimeApi = chrome.runtime;
                          chrome.runtime = 'bar';
                          chrome.test.assertEq('bar', chrome.runtime);

                          // Verify that `browser.runtime` was NOT affected by
                          // the change to `chrome.runtime`, confirming it's an
                          // independent object instance.
                          chrome.test.assertEq(originalRuntimeApi,
                                               browser.runtime);

                          // Clean up for subsequent tests.
                          chrome.runtime = originalRuntimeApi;
                          chrome.test.assertEq(originalRuntimeApi,
                                               chrome.runtime);

                          // Modify a member of chrome.runtime and confirm
                          // browser.runtime reflects that change, because
                          // both independent binding objects point to the
                          // same underlying API implementation.
                          chrome.runtime.sendMessage = 'bar';
                          chrome.test.assertEq('bar',
                                               chrome.runtime.sendMessage);
                          chrome.test.assertEq(chrome.runtime.sendMessage,
                                               browser.runtime.sendMessage);

                          chrome.test.succeed();
                        }
                      ]);)");

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that `browser.devtools` is aliased to `chrome.devtools` in a devtools
// context. This is unique because `devtools` APIs are injected by the devtools
// frontend rather than the standard extension bindings system.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_DevToolsApiAliasing) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
          "name": "DevTools Aliasing test",
          "version": "0.1",
          "manifest_version": 3,
          "devtools_page": "devtools.html"
        })");
  test_dir.WriteFile(FILE_PATH_LITERAL("devtools.html"),
                     "<script src='devtools.js'></script>");
  test_dir.WriteFile(FILE_PATH_LITERAL("devtools.js"),
                     R"(chrome.test.runTests([
                        function checkDevtoolsApiAliasing() {
                          chrome.test.assertTrue(browser.devtools !== undefined,
                                                 'browser.devtools');
                          // Unlike other APIs, `browser.devtools` is a dynamic
                          // alias (via a getter) to `chrome.devtools`. This is
                          // necessary because `devtools` is injected by the
                          // devtools frontend.
                          chrome.test.assertEq(chrome.devtools,
                                               browser.devtools);

                          // Attempt to overwrite the root `chrome.devtools`
                          // object. In this context (devtools page), it is
                          // non-writable/configurable.
                          let originalDevtoolsApi = chrome.devtools;
                          chrome.devtools = 'bar';
                          chrome.test.assertEq(originalDevtoolsApi,
                                               chrome.devtools);

                          // Since `browser.devtools` is a getter that looks up
                          // `chrome.devtools`, it still matches whatever is on
                          // `chrome`.
                          chrome.test.assertEq(chrome.devtools,
                                               browser.devtools);

                          // Modify a member of chrome.devtools and confirm
                          // browser.devtools reflects that change, because
                          // it is a direct dynamic alias to the same
                          // underlying object.
                          chrome.devtools.panels = 'bar';
                          chrome.test.assertEq('bar', browser.devtools.panels);

                          chrome.test.succeed();
                        }
                      ]);)");

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  DevToolsWindow::OpenDevToolsWindow(GetActiveWebContents(),
                                     DevToolsToggleAction::Show(),
                                     DevToolsOpenedByAction::kUnknown);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Tests that `devtools` is NOT available if the extension doesn't have a
// devtools page.
IN_PROC_BROWSER_TEST_F(NativeBindingsBrowserNamespaceTest,
                       ChromeAndBrowserObjects_NoDevTools) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      R"({
          "name": "No DevTools test extension",
          "version": "0.1",
          "manifest_version": 3
        })");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"),
                     "<script src='page.js'></script>");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.js"),
                     R"(chrome.test.runTests([
                        function checkNoDevTools() {
                          chrome.test.assertEq(undefined, chrome.devtools);
                          chrome.test.assertEq(undefined, browser.devtools);
                          chrome.test.succeed();
                        }
                      ]);)");

  ResultCatcher catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to the extension page to run devtools tests.
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            extension->GetResourceURL("page.html")));

  // Wait for the devtools tests to run.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(crbug.com/401226626): Test that the browser object also has dev mode
// restricted APIs set on correctly as well.

class DeveloperModeNativeBindingsApiTest
    : public NativeBindingsApiTest,
      public testing::WithParamInterface<bool> {
 public:
  DeveloperModeNativeBindingsApiTest() {
    if (GetParam()) {
      // Ensure chrome.debugger is controlled by Developer Mode.
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/
          {extensions_features::kUserScriptUserExtensionToggle,
           extensions_features::kDebuggerAPIRestrictedToDevMode},
          /*disabled_features=*/{});

    } else {
      // Ensure chrome.userScripts is controlled by Developer Mode.
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{}, /*disabled_features=*/{
              extensions_features::kUserScriptUserExtensionToggle,
              extensions_features::kDebuggerAPIRestrictedToDevMode});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/390138269): Revert the user scripts specific testing once the
// extensions::kUserScriptUserExtensionToggle feature is launched.
IN_PROC_BROWSER_TEST_P(
    DeveloperModeNativeBindingsApiTest,
    DeveloperModeOnlyWithAPIPermissionUserIsNotInDeveloperMode) {
  // Developer mode-only APIs should not be available if the user is not in
  // developer mode.
  SetCustomArg("not_in_developer_mode");
  util::SetDeveloperModeForProfile(profile(), false);
  if (GetParam()) {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/developer_mode_only_with_api_permission"))
        << message_;
  } else {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/developer_mode_only_with_user_scripts_api_permission"))
        << message_;
  }
}

IN_PROC_BROWSER_TEST_P(
    DeveloperModeNativeBindingsApiTest,
    DeveloperModeOnlyWithAPIPermissionUserIsInDeveloperMode) {
  // Developer mode-only APIs should be available if the user is in developer
  // mode.
  SetCustomArg("in_developer_mode");
  util::SetDeveloperModeForProfile(profile(), true);
  if (GetParam()) {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/developer_mode_only_with_api_permission"))
        << message_;
  } else {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/developer_mode_only_with_user_scripts_api_permission"))
        << message_;
  }
}

IN_PROC_BROWSER_TEST_P(
    DeveloperModeNativeBindingsApiTest,
    DeveloperModeOnlyWithoutAPIPermissionUserIsNotInDeveloperMode) {
  util::SetDeveloperModeForProfile(profile(), false);
  if (GetParam()) {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/developer_mode_only_without_api_permission"))
        << message_;
  } else {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/"
        "developer_mode_only_without_user_scripts_api_permission"))
        << message_;
  }
}

IN_PROC_BROWSER_TEST_P(
    DeveloperModeNativeBindingsApiTest,
    DeveloperModeOnlyWithoutAPIPermissionUserIsInDeveloperMode) {
  util::SetDeveloperModeForProfile(profile(), true);
  if (GetParam()) {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/developer_mode_only_without_api_permission"))
        << message_;
  } else {
    ASSERT_TRUE(RunExtensionTest(
        "native_bindings/"
        "developer_mode_only_without_user_scripts_api_permission"))
        << message_;
  }
}

// Tests that changing the developer mode setting affects existing renderers
// for page-based contexts (i.e., the main renderer thread).
IN_PROC_BROWSER_TEST_P(DeveloperModeNativeBindingsApiTest,
                       SwitchingDeveloperModeAffectsExistingRenderers_Pages) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["%s"]
         })";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
         <html>
           <script src="page.js"></script>
         </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifest, GetParam() ? "debugger" : "userScripts"));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("page.js"),
      GetParam() ? kCheckApiAvailability : kCheckApiAvailabilityUserScripts);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), "// blank");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  const GURL extension_url = extension->GetResourceURL("page.html");

  // Navigate to the extension page.
  auto* existing_tab = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(existing_tab, extension_url));
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
IN_PROC_BROWSER_TEST_P(DeveloperModeNativeBindingsApiTest,
                       IncognitoRenderersUseOriginalProfilesDevModeSetting) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "incognito": "split",
           "permissions": ["%s"]
         })";
  static constexpr char kPageHtml[] =
      R"(<!doctype html>
         <html>
           <script src="page.js"></script>
         </html>)";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifest, GetParam() ? "debugger" : "userScripts"));
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPageHtml);
  test_dir.WriteFile(
      FILE_PATH_LITERAL("page.js"),
      GetParam() ? kCheckApiAvailability : kCheckApiAvailabilityUserScripts);
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
IN_PROC_BROWSER_TEST_P(
    DeveloperModeNativeBindingsApiTest,
    DISABLED_SwitchingDeveloperModeAffectsExistingRenderers_ServiceWorkers) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "permissions": ["%s"],
           "background": {"service_worker": "background.js"}
         })";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(
      base::StringPrintf(kManifest, GetParam() ? "debugger" : "userScripts"));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("background.js"),
      GetParam() ? kCheckApiAvailability : kCheckApiAvailabilityUserScripts);
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

INSTANTIATE_TEST_SUITE_P(All,
                         DeveloperModeNativeBindingsApiTest,
                         // extensions_features::kDebuggerAPIRestrictedToDevMode
                         testing::Bool());

}  // namespace extensions
