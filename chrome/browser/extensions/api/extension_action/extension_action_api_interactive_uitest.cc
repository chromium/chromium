// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// An interactive UI test suite for the chrome.action API. This is used for
// tests where things like focus and window activation are important.
class ActionAPIInteractiveUITest : public ExtensionApiTest {
 public:
  ActionAPIInteractiveUITest() = default;
  ~ActionAPIInteractiveUITest() override = default;

  // Loads a common "stub" extension with an action specified in its manifest;
  // tests then execute script in this extension's context.
  // This allows us to more seamlessly integrate C++ and JS execution by
  // inlining the JS here in this file, rather than needing to separate it out
  // into separate test files and coordinate messaging back-and-forth.
  const Extension* LoadStubExtension() {
    return LoadExtension(
        test_data_dir_.AppendASCII("extension_action/stub_action"));
  }

  // Runs the given `script` in the background service worker context for the
  // `extension`, and waits for a corresponding test success notification.
  void RunScriptTest(const std::string& script, const Extension& extension) {
    ResultCatcher result_catcher;
    BackgroundScriptExecutor::ExecuteScriptAsync(profile(), extension.id(),
                                                 script);
    EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  }

  // Same as `RunScriptTest()`, but wraps `script` in a test.runTests() call
  // with a single function.
  void WrapAndRunScript(const std::string& script, const Extension& extension) {
    constexpr char kTemplate[] =
        R"(chrome.test.runTests([
             async function openPopupTest() {
               %s
             }
           ]);)";
    std::string wrapped_script = base::StringPrintf(kTemplate, script.c_str());
    RunScriptTest(wrapped_script, extension);
  }

  // Returns the active popup for the given `extension`, if one exists.
  ExtensionHost* GetPopup(const Extension& extension) {
    ExtensionHostRegistry* registry = ExtensionHostRegistry::Get(profile());
    std::vector<ExtensionHost*> hosts =
        registry->GetHostsForExtension(extension.id());
    ExtensionHost* found_host = nullptr;
    for (auto* host : hosts) {
      if (host->extension_host_type() == mojom::ViewType::kExtensionPopup) {
        if (found_host) {
          ADD_FAILURE() << "Multiple popups found!";
          return nullptr;
        }
        found_host = host;
      }
    }
    return found_host;
  }

  // Returns true if the given `browser` has an active popup.
  bool BrowserHasPopup(Browser* browser) {
    return ExtensionActionTestHelper::Create(browser)->HasPopup();
  }

  // The action.openPopup() function is currently scoped to dev channel.
  ScopedCurrentChannel scoped_current_channel_{version_info::Channel::DEV};
};

// Tests displaying a popup in the active window when no window ID is specified.
IN_PROC_BROWSER_TEST_F(ActionAPIInteractiveUITest, OpenPopupInActiveWindow) {
  const Extension* extension = LoadStubExtension();
  ASSERT_TRUE(extension);

  constexpr char kScript[] =
      R"(await chrome.action.openPopup();
         chrome.test.succeed();)";
  WrapAndRunScript(kScript, *extension);

  EXPECT_TRUE(BrowserHasPopup(browser()));
  ExtensionHost* host = GetPopup(*extension);
  ASSERT_TRUE(host);
  EXPECT_TRUE(content::WaitForLoadStop(host->host_contents()));
  EXPECT_TRUE(host->has_loaded_once());
  EXPECT_EQ(extension->GetResourceURL("popup.html"),
            host->main_frame_host()->GetLastCommittedURL());
}

// Tests displaying a popup in a window specified in the API call.
IN_PROC_BROWSER_TEST_F(ActionAPIInteractiveUITest, OpenPopupInSpecifiedWindow) {
  const Extension* extension = LoadStubExtension();
  ASSERT_TRUE(extension);

  Browser* second_browser = CreateBrowser(profile());
  ASSERT_TRUE(second_browser);
  ui_test_utils::BrowserActivationWaiter(second_browser).WaitForActivation();

  EXPECT_FALSE(browser()->window()->IsActive());
  EXPECT_TRUE(second_browser->window()->IsActive());

  int window_id = ExtensionTabUtil::GetWindowId(second_browser);

  constexpr char kScript[] =
      R"(await chrome.action.openPopup({windowId: %d});
         chrome.test.succeed();)";
  WrapAndRunScript(base::StringPrintf(kScript, window_id), *extension);

  // The popup should be shown on the second browser.
  {
    EXPECT_TRUE(BrowserHasPopup(second_browser));
    ExtensionHost* host = GetPopup(*extension);
    ASSERT_TRUE(host);
    EXPECT_TRUE(content::WaitForLoadStop(host->host_contents()));
    EXPECT_TRUE(host->has_loaded_once());
    EXPECT_EQ(extension->GetResourceURL("popup.html"),
              host->main_frame_host()->GetLastCommittedURL());
  }

  EXPECT_FALSE(BrowserHasPopup(browser()));
}

// Tests displaying a popup in an inactive window specified in the API call.
IN_PROC_BROWSER_TEST_F(ActionAPIInteractiveUITest, OpenPopupInInactiveWindow) {
  const Extension* extension = LoadStubExtension();
  ASSERT_TRUE(extension);

  Browser* second_browser = CreateBrowser(profile());
  ASSERT_TRUE(second_browser);
  ui_test_utils::BrowserActivationWaiter(second_browser).WaitForActivation();

  // TODO(crbug.com/40057101): We should allow extensions to open a
  // popup in an inactive window. Currently, this fails, so try to open the
  // popup in the active window (but with a specified ID).
  EXPECT_FALSE(browser()->window()->IsActive());
  EXPECT_TRUE(second_browser->window()->IsActive());

  int inactive_window_id = ExtensionTabUtil::GetWindowId(browser());

  // The popup should fail to show on the first (inactive) browser.
  constexpr char kFailureScript[] =
      R"(await chrome.test.assertPromiseRejects(
         chrome.action.openPopup({windowId: %d}),
         /Error: Cannot show popup for an inactive window./);
       chrome.test.succeed();)";
  WrapAndRunScript(base::StringPrintf(kFailureScript, inactive_window_id),
                   *extension);

  EXPECT_FALSE(BrowserHasPopup(browser()));
}

// Tests a series of action.openPopup() invocations that are expected to fail.
IN_PROC_BROWSER_TEST_F(ActionAPIInteractiveUITest, OpenPopupFailures) {
  const Extension* extension = LoadStubExtension();
  ASSERT_TRUE(extension);

  constexpr char kScript[] =
      R"(chrome.test.runTests([
           async function openPopupFailsWithFakeWindow() {
             const fakeWindowId = 99999;
             await chrome.test.assertPromiseRejects(
                 chrome.action.openPopup({windowId: fakeWindowId}),
                 `Error: No window with id: ${fakeWindowId}.`);
             chrome.test.succeed();
           },
           async function openPopupFailsWhenNoPopupSpecified() {
             // Specifying an empty string for the popup means "no popup".
             await chrome.action.setPopup({popup: ''});
             await chrome.test.assertPromiseRejects(
                 chrome.action.openPopup(),
                 'Error: Extension does not have a popup on the active tab.');
             chrome.test.succeed();
           },
           async function openPopupFailsWhenPopupIsDisabled() {
             await chrome.action.setPopup({popup: 'popup.html'});
             await chrome.action.disable();
             await chrome.test.assertPromiseRejects(
                 chrome.action.openPopup(),
                 'Error: Extension does not have a popup on the active tab.');
             chrome.test.succeed();
           },
           async function tryToOpenPopupThatClosesBeforeItsOpened() {
             await chrome.action.enable();
             // insta_close_popup.html has a script file that synchronously
             // calls window.close() during document parsing. This results in
             // the popup closing before it's actually shown.
             await chrome.action.setPopup({popup: 'insta_close_popup.html'});
             await chrome.test.assertPromiseRejects(
                 chrome.action.openPopup(),
                 'Error: Failed to open popup.');
             chrome.test.succeed();
           },
         ]);)";
  RunScriptTest(kScript, *extension);
  EXPECT_FALSE(BrowserHasPopup(browser()));
}

// Tests that openPopup() will not succeed if a popup is only visible on a tab
// because of a declarative condition.
// https://crbug.com/1289846.
IN_PROC_BROWSER_TEST_F(ActionAPIInteractiveUITest,
                       DontOpenPopupForDeclarativelyShownAction) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  constexpr char kManifest[] =
      R"({
           "name": "My Extension",
           "manifest_version": 3,
           "version": "0.1",
           "background": { "service_worker": "worker.js" },
           "action": { "default_popup": "popup.html" },
           "permissions": ["declarativeContent"]
         })";
  constexpr char kWorkerJs[] = "// Intentionally blank";
  constexpr char kPopupHtml[] = "<html>Hello, World!</html>";

  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerJs);
  test_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), kPopupHtml);

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  constexpr char kDisableActionAndSetRule[] =
      R"(await chrome.action.disable();
         await chrome.declarativeContent.onPageChanged.addRules([{
             conditions: [
               new chrome.declarativeContent.PageStateMatcher({
                 pageUrl: {hostEquals: 'example.com'}
               })
             ],
             actions: [
               new chrome.declarativeContent.ShowAction(),
             ]
         }]);
         chrome.test.succeed();)";

  WrapAndRunScript(kDisableActionAndSetRule, *extension);

  GURL url = embedded_test_server()->GetURL("example.com", "/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetActiveWebContents());

  const ExtensionAction* extension_action =
      ExtensionActionManager::Get(profile())->GetExtensionAction(*extension);
  ASSERT_TRUE(extension_action);
  EXPECT_TRUE(extension_action->GetIsVisible(tab_id));
  EXPECT_FALSE(extension_action->GetIsVisibleIgnoringDeclarative(tab_id));

  constexpr char kTryOpenPopup[] =
      R"(await chrome.test.assertPromiseRejects(
             chrome.action.openPopup(),
             'Error: Extension does not have a popup on the active tab.');
         chrome.test.succeed();)";
  WrapAndRunScript(kTryOpenPopup, *extension);
}

}  // namespace extensions
