// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/gfx/native_widget_types.h"

namespace extensions {
using PermissionsApiInteractiveTest = ExtensionApiTest;

// Tests that the dialog is parented to the correct window when there are
// multiple browser windows open. Regression test for crbug.com/41482206.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// BringBrowserWindowToFront hangs on Linux: http://crbug.com/356183782
#define MAYBE_DialogWithMultipleWindows DISABLED_DialogWithMultipleWindows
#else
#define MAYBE_DialogWithMultipleWindows DialogWithMultipleWindows
#endif
IN_PROC_BROWSER_TEST_F(PermissionsApiInteractiveTest,
                       MAYBE_DialogWithMultipleWindows) {
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  auto dialog_action_reset =
      PermissionsRequestFunction::SetDialogActionForTests(
          PermissionsRequestFunction::DialogAction::kProgrammatic);

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("permissions/optional_request_from_popup"));

  Browser* first_browser = browser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_browser));

  // Create a second browser window and wait for activation.
  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_NE(first_browser, second_browser);
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(second_browser));

  // Activate the first browser (again).
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(first_browser));

  // Simulate a click on the extension action. The extension expects back a
  // reply from the browser. On receiving the reply, it will request additional
  // permissions.
  std::unique_ptr<ExtensionActionTestHelper> action_helper =
      ExtensionActionTestHelper::Create(first_browser);
  ExtensionTestMessageListener popup_listener("popup_loaded",
                                              ReplyBehavior::kWillReply);
  action_helper->Press(extension->id());
  ASSERT_TRUE(popup_listener.WaitUntilSatisfied());

  // We must wait for the popup to be activated, before asking it to request
  // additional permissions.
  action_helper->WaitForPopup();

  // Configure a custom show dialog callback to capture the parent_window. This
  // callback is our actual test, which validates the dialog's parent window.
  base::RunLoop run_loop;
  auto show_dialog_callback = [&](gfx::NativeWindow parent_window) {
    EXPECT_EQ(parent_window, first_browser->window()->GetNativeWindow());
    run_loop.Quit();
  };

  auto bound_callback = base::BindLambdaForTesting(show_dialog_callback);
  auto show_dialog_callback_reset =
      PermissionsRequestFunction::SetShowDialogCallbackForTests(
          &bound_callback);

  // At this point, the popup is active and our test hooks are configured. Ask
  // the popup to request additional permissions and wait for our test callback.
  popup_listener.Reply("request_permissions");
  run_loop.Run();

  PermissionsRequestFunction::ResolvePendingDialogForTests(
      /*accept_dialog=*/true);
}
}  // namespace extensions
