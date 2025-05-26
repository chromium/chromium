// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/extensions/api/user_scripts/user_scripts_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace extensions {

class UserScriptsUITest : public InteractiveBrowserTestT<UserScriptsAPITest> {
 public:
  // Checks that toggle is `!enabled` and then toggles it to `enabled` state.
  auto CheckCurrentToggleStateAndThenToggleItInUI(
      ui::ElementIdentifier page_id,
      const DeepQuery toggle_dom_path,
      bool enabled) {
    return Steps(
        // ClickElement() won't click something off screen so scroll the toggle
        // into view in case it is not.
        ScrollIntoView(page_id, toggle_dom_path),
        // Check the toggle is `!enabled`.
        EnsurePresent(page_id, toggle_dom_path),
        CheckJsResultAt(page_id, toggle_dom_path, "(el) => el.checked",
                        !enabled),
        // Click the toggle and check it is `enabled`.
        ClickElement(page_id, toggle_dom_path),
        CheckJsResultAt(page_id, toggle_dom_path, "(el) => el.checked",
                        enabled));
  }

  // Toggles the extensions_features::kUserScriptUserExtensionToggle toggle to
  // `enabled` state.
  auto TogglePerExtensionToggleInUI(ui::ElementIdentifier page_id,
                                    const ExtensionId& extension_id,
                                    bool enabled) {
    const DeepQuery kPathToUserScriptsToggle{
        "extensions-manager",
        "extensions-detail-view",
        "extensions-toggle-row#allow-user-scripts",
        "cr-toggle#crToggle",
    };

    // Enable the per-extension toggle in the UI.
    return Steps(
        // Navigate to the extensions detail page for the extension (where the
        // toggle lives).
        NavigateWebContents(page_id,
                            GURL(base::StrCat({chrome::kChromeUIExtensionsURL,
                                               "?id=", extension_id.c_str()}))),
        CheckCurrentToggleStateAndThenToggleItInUI(
            page_id, kPathToUserScriptsToggle, enabled));
  }

  // Toggles the (non extensions_features::kUserScriptUserExtensionToggle state)
  // dev mode toggle `enabled` state.
  auto ToggleDevModeInUI(ui::ElementIdentifier page_id, bool enabled) {
    const DeepQuery kPathToDevModeToggle{
        "extensions-manager extensions-toolbar",
        "cr-toggle#devMode",
    };

    // Enable dev mode toggle in the UI.
    return Steps(
        // Navigate to the extensions detail page for the extension.
        NavigateWebContents(page_id, GURL(chrome::kChromeUIExtensionsURL)),
        CheckCurrentToggleStateAndThenToggleItInUI(
            page_id, kPathToDevModeToggle, enabled));
  }

  // Checks that the chrome.userScripts API is not available in the background
  // script.
  auto VerifyUserScriptsIsNotAvailable(const ExtensionId& extension_id) {
    // Register the user script in the extension background script and confirm
    // it registered successfully.
    return CheckResult(
        [this, &extension_id]() -> bool {
          return BackgroundScriptExecutor::ExecuteScript(
                     profile(), extension_id, "verifyApiIsNotAvailable();",
                     BackgroundScriptExecutor::ResultCapture::
                         kSendScriptResult) == "success";
        },
        true, "Checking that the userScripts API is not available");
  }

  // Registers a dynamic user script with the chrome.userScripts API.
  auto RegisterUserScript(const ExtensionId& extension_id) {
    // Register the user script in the extension background script and confirm
    // it registered successfully.
    return CheckResult(
        [this, &extension_id]() -> bool {
          return BackgroundScriptExecutor::ExecuteScript(
                     profile(), extension_id, "registerUserScripts();",
                     BackgroundScriptExecutor::ResultCapture::
                         kSendScriptResult) == "success";
        },
        true, "Registering dynamic user script and checking that it completed");
  }
};

// TODO(crbug.com/416377497): Re-enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ToggleControls_UserScriptsAPIUsage \
  DISABLED_ToggleControls_UserScriptsAPIUsage
#else
#define MAYBE_ToggleControls_UserScriptsAPIUsage \
  ToggleControls_UserScriptsAPIUsage
#endif
// Tests the toggling the UI toggle (dependent on feature) controls whether the
// user has allowed userScripts API usage.
IN_PROC_BROWSER_TEST_P(UserScriptsUITest,
                       MAYBE_ToggleControls_UserScriptsAPIUsage) {
  // Load extension that has API permission to use the userScripts API, but not
  // the per-extension toggle for userScripts enabled.
  ExtensionTestMessageListener extension_background_started_listener =
      ExtensionTestMessageListener("started");
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("user_scripts/allowed_tests"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(extension_background_started_listener.WaitUntilSatisfied());

  const DeepQuery kPathToUserScriptInjectedDiv{
      "#user-script-code",
  };

  // This test verifies that:
  //   1) The userScripts API is initially unavailable.
  //   2) Enabling the toggle allows for a dynamic user script can be registered
  //      and injected.
  //   3) Disabling the toggle causes user scripts to no longer inject
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTab);
  RunTestSequence(
      InstrumentTab(kTab),

      VerifyUserScriptsIsNotAvailable(extension->id()),

      // Enable the toggle depending on feature state.
      GetParam() ? TogglePerExtensionToggleInUI(kTab, extension->id(),
                                                /*enabled=*/true)
                 : ToggleDevModeInUI(kTab, /*enabled=*/true),

      RegisterUserScript(extension->id()),

      // Navigate tab to a webpage where the user script should inject a <div>.
      NavigateWebContents(
          kTab, embedded_test_server()->GetURL("example.com", "/simple.html")),
      // Ensure the user script injected its <div>.
      EnsurePresent(kTab, kPathToUserScriptInjectedDiv),

      // Disable the toggle depending on feature state.
      GetParam() ? TogglePerExtensionToggleInUI(kTab, extension->id(),
                                                /*enabled=*/false)
                 : ToggleDevModeInUI(kTab, /*enabled=*/false),
      // Navigate tab to a webpage where the user script should no longer inject
      // a <div>.
      NavigateWebContents(
          kTab, embedded_test_server()->GetURL("example.com", "/simple.html")),
      // Ensure the user script no longer injects its <div>.
      EnsureNotPresent(kTab, kPathToUserScriptInjectedDiv));
}

INSTANTIATE_TEST_SUITE_P(PerExtensionToggle,
                         UserScriptsUITest,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Values("true"));
INSTANTIATE_TEST_SUITE_P(DevModeToggle,
                         UserScriptsUITest,
                         // extensions_features::kUserScriptUserExtensionToggle
                         testing::Values("false"));

}  // namespace extensions
