// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/service_worker_test_helpers.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/service_worker/service_worker_test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace extensions {

namespace {

class InputImeApiTest : public ExtensionApiTest {
 public:
  InputImeApiTest() = default;
  InputImeApiTest(const InputImeApiTest&) = delete;
  InputImeApiTest& operator=(const InputImeApiTest&) = delete;
  ~InputImeApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    // The test extension needs chrome.inputMethodPrivate to set up
    // the test.
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    "ilanclmaeigfpnmdlgelmhkpkegdioip");
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(InputImeApiTest, Basic) {
  // Enable the test IME from the test extension.
  std::vector<std::string> extension_ime_ids{
      "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest"};
  ash::input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->SetEnabledExtensionImes(extension_ime_ids);
  ASSERT_TRUE(RunExtensionTest("input_ime/basic")) << message_;
}

// Tests that if an extension service worker shuts down due to idle timeout that
// new keypresses will wakeup the worker and send `OnKeyEvent` event message to
// the worker's listener.
// Regression test for crbug.com/441317290.
IN_PROC_BROWSER_TEST_F(InputImeApiTest, WakeWorkerAfterShutdown) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Enable the test extension to be an IME.
  std::vector<std::string> extension_ime_ids{
      "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest"};
  ash::input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->SetEnabledExtensionImes(extension_ime_ids);

  ExtensionTestMessageListener set_current_input_method("set_input_success");
  service_worker_test_utils::TestServiceWorkerContextObserver
      worker_stopped_observer(profile());
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("input_ime/worker_shutdown"),
      {.wait_for_renderers = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  const int64_t worker_version_id =
      worker_stopped_observer.WaitForWorkerStarted();

  // Confirm the current input method has been set to the test extension.
  ASSERT_TRUE(set_current_input_method.WaitUntilSatisfied());

  // Open page with a text box.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/test_file_with_text_field.html")));

  // Simulate idle timeout to terminate the service worker. We must do this
  // after navigating to the text field page because the navigation causes the
  // worker to restart.
  content::ServiceWorkerContext* context =
      util::GetServiceWorkerContextForExtensionId(extension->id(), profile());
  content::SetServiceWorkerIdleDelay(context, worker_version_id,
                                     base::Seconds(0));
  worker_stopped_observer.WaitForWorkerStopped();

  // Focus the text field.
  content::WebContents* tab = GetActiveWebContents();
  {
    SCOPED_TRACE(
        "waiting for textField to appear so we can focus the cursor on it");
    // Using a Promise allows us to wait until the element is available on the
    // page to avoid test flakiness when trying to focus.
    ASSERT_TRUE(content::ExecJs(tab,
                                "new Promise(resolve => {"
                                "  const check = () => {"
                                "    const elem = "
                                "document.getElementById('textField');"
                                "    if (elem) {"
                                "      elem.focus();"
                                "      resolve();"
                                "    } else {"
                                "      setTimeout(check, 10);"
                                "    }"
                                "  };"
                                "  check();"
                                "});"));  // Assert the text box is focused.
  }
  ASSERT_EQ(
      true,
      content::EvalJsAfterLifecycleUpdate(
          tab,
          /*raf_script=*/"",
          "document.activeElement === document.getElementById('textField');")
          .ExtractBool());

  service_worker_test_utils::TestServiceWorkerContextObserver
      worker_restarted_observer(profile());
  ExtensionTestMessageListener listener_finished("ime_listener_finished");

  // Type a lowercase "j" character into the text box and confirm it's in the
  // text field.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_J, /*control=*/false,
      /*shift=*/false, /*alt=*/false, /*command=*/false));

  {
    SCOPED_TRACE(
        "waiting for worker to restart in response to the onKeyEvent event");
    const int64_t restarted_worker_version_id =
        worker_restarted_observer.WaitForWorkerStarted();
    ASSERT_EQ(worker_version_id, restarted_worker_version_id);
  }

  // TODO(crbug.com/441317290): This should be enabled when the bug is fixed.
  // Wait for the API method to complete executing, and allow time for the page
  // UI to update with the replaced key.
  // {
  //   SCOPED_TRACE("waiting for worker to finish processing onKeyEvent event");
  //   ASSERT_TRUE(listener_finished.WaitUntilSatisfied());
  // }

  // TODO(crbug.com/441317290): This should be EXPECT_EQ when the bug is fixed.
  // Verify the text in the text box changed (to uppercase).
  EXPECT_NE("J", content::EvalJsAfterLifecycleUpdate(
                     tab, /*raf_script=*/"",
                     "document.getElementById('textField').value")
                     .ExtractString());
}

class InputImeKeyboardEventDataApiTest : public base::test::WithFeatureOverride,
                                         public InputImeApiTest {
 public:
  struct TestCase {
    ui::KeyboardCode key_code;
    std::string expected_dom_key;
  };

  InputImeKeyboardEventDataApiTest()
      : base::test::WithFeatureOverride(
            extensions_features::kInputImeKeyboardEventDataToDOMSpec) {
    if (IsParamFeatureEnabled()) {
      test_cases_ = {
          {ui::VKEY_CONTROL, "Control"},
          {ui::VKEY_ESCAPE, "Escape"},
          {ui::VKEY_DOWN, "ArrowDown"},
          {ui::VKEY_UP, "ArrowUp"},
          {ui::VKEY_LEFT, "ArrowLeft"},
          {ui::VKEY_RIGHT, "ArrowRight"},
          {ui::VKEY_BROWSER_BACK, "BrowserBack"},
          {ui::VKEY_BROWSER_FORWARD, "BrowserForward"},
          // ChromeOS defaults F<#> keys to media keys.
          {ui::VKEY_F1, "BrowserBack"},
          {ui::VKEY_F2, "BrowserForward"},

      };
    } else {
      test_cases_ = {
          {ui::VKEY_CONTROL, "Ctrl"},
          {ui::VKEY_ESCAPE, "Esc"},
          {ui::VKEY_DOWN, "Down"},
          {ui::VKEY_UP, "Up"},
          {ui::VKEY_LEFT, "Left"},
          {ui::VKEY_RIGHT, "Right"},
          {ui::VKEY_BROWSER_BACK, "HistoryBack"},
          {ui::VKEY_BROWSER_FORWARD, "HistoryForward"},
          // ChromeOS defaults F<#> keys to media keys.
          {ui::VKEY_F1, "HistoryBack"},
          {ui::VKEY_F2, "HistoryForward"},
      };
    }
  }

 protected:
  std::vector<TestCase> test_cases() { return test_cases_; }

 private:
  std::vector<TestCase> test_cases_;
};

// Tests that a sampling of keys are sent through with the expected
// `KeyboardEvent.key` values according to the
// `extensions_features::kInputImeKeyboardEventDataToDOMSpec` feature state.
// Regression test for crbug.com/467185174.
IN_PROC_BROWSER_TEST_P(InputImeKeyboardEventDataApiTest,
                       DOMKeyboardEventCodeValues) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Enable the test extension to be an IME.
  std::vector<std::string> extension_ime_ids{
      "_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest"};
  ash::input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->SetEnabledExtensionImes(extension_ime_ids);

  // Load the extension and verify it is the current IME.
  ExtensionTestMessageListener set_current_input_method("set_input_success");
  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("input_ime/control_key"),
      {.wait_for_renderers = true, .wait_for_registration_stored = true});
  ASSERT_TRUE(extension);
  // Confirm the current input method has been set to the test extension.
  ASSERT_TRUE(set_current_input_method.WaitUntilSatisfied());

  // Navigate to a page and focus the text input.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/test_file_with_text_field.html")));
  // Focus and then assert text field is focused.
  content::WebContents* tab = GetActiveWebContents();
  {
    SCOPED_TRACE(
        "waiting for textField to appear so we can focus the cursor on it");
    // Using a Promise allows us to wait until the element is available on the
    // page to avoid test flakiness when trying to focus.
    ASSERT_TRUE(content::ExecJs(tab,
                                "new Promise(resolve => {"
                                "  const check = () => {"
                                "    const elem = "
                                "document.getElementById('textField');"
                                "    if (elem) {"
                                "      elem.focus();"
                                "      resolve();"
                                "    } else {"
                                "      setTimeout(check, 10);"
                                "    }"
                                "  };"
                                "  check();"
                                "});"));  // Assert the text box is focused.
  }
  ASSERT_EQ(
      true,
      content::EvalJsAfterLifecycleUpdate(
          tab,
          /*raf_script=*/"",
          "document.activeElement === document.getElementById('textField');")
          .ExtractBool());

  // Send a series of keys and validate that the `keyData.code` matches the
  // expected KeyboardEvent.code.
  for (const auto& test_case : test_cases()) {
    // Send the key code to the page which then is sent as an
    // `input.ime.OnKeyEvent()` to the extension.
    ExtensionTestMessageListener key_listener;
    key_listener.set_extension_id(extension->id());
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser(), test_case.key_code, /*control=*/false,
        /*shift=*/false, /*alt=*/false, /*command=*/false));
    {
      SCOPED_TRACE(base::StringPrintf(
          "waiting for key code %hu to be processed by the "
          "extension and the keyData.key (%s) to be sent back",
          test_case.key_code, test_case.expected_dom_key));
      ASSERT_TRUE(key_listener.WaitUntilSatisfied());
    }

    // Verify the returned key matches expectations.
    const std::string& key = key_listener.message();
    ASSERT_FALSE(key.empty());
    EXPECT_EQ(test_case.expected_dom_key, key)
        << "Mismatch for key code: " << test_case.key_code
        << " expected key: " << test_case.expected_dom_key
        << " but got key: " << key;
  }
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(InputImeKeyboardEventDataApiTest);

}  // namespace extensions
