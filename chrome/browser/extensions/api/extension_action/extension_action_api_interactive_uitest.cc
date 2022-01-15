// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
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
    base::RunLoop run_loop;
    auto callback = [&run_loop](base::Value value) { run_loop.Quit(); };
    browsertest_util::ExecuteScriptInServiceWorker(
        profile(), extension.id(), script,
        base::BindLambdaForTesting(callback));
    run_loop.Run();
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

  // TODO(https://crbug.com/1245093): We should allow extensions to open a
  // popup in an inactive window. Currently, this fails, so try to open the
  // popup in the active window (but with a specified ID).
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
    EXPECT_TRUE(host->has_loaded_once());
    EXPECT_EQ(extension->GetResourceURL("popup.html"),
              host->main_frame_host()->GetLastCommittedURL());
  }

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
         ]);)";
  RunScriptTest(kScript, *extension);
  EXPECT_FALSE(BrowserHasPopup(browser()));
}

}  // namespace extensions
