// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "extensions/common/extension_id.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
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
  ASSERT_TRUE(RunExtensionTest("input_ime")) << message_;
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
      test_data_dir_.AppendASCII("input_ime_worker_shutdown"),
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
  ASSERT_TRUE(
      content::ExecJs(tab, "document.getElementById('textField').focus();"));
  // Assert the text box is focused.
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

}  // namespace extensions
