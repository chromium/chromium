// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/app_modal/native_app_modal_dialog.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

void GetNextDialog(app_modal::NativeAppModalDialog** native_dialog) {
  DCHECK(native_dialog);
  *native_dialog = nullptr;
  app_modal::JavaScriptAppModalDialog* dialog =
      ui_test_utils::WaitForAppModalDialog();
  *native_dialog = dialog->native_dialog();
  ASSERT_TRUE(*native_dialog);
}

void CloseDialog() {
  app_modal::NativeAppModalDialog* dialog = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetNextDialog(&dialog));
  dialog->CloseAppModalDialog();
}

void AcceptDialog() {
  app_modal::NativeAppModalDialog* dialog = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetNextDialog(&dialog));
  dialog->AcceptAppModalDialog();
}

void CancelDialog() {
  app_modal::NativeAppModalDialog* dialog = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetNextDialog(&dialog));
  dialog->CancelAppModalDialog();
}

void CheckAlertResult(const std::string& dialog_name,
                      size_t* call_count,
                      base::Value value) {
  ASSERT_TRUE(value.is_none());
  ++*call_count;
}

void CheckConfirmResult(const std::string& dialog_name,
                        bool expected_value,
                        size_t* call_count,
                        base::Value value) {
  ASSERT_TRUE(value.is_bool()) << dialog_name;
  ASSERT_EQ(expected_value, value.GetBool()) << dialog_name;
  ++*call_count;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlertBasic) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  const Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = ProcessManager::Get(browser()->profile())
                            ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(host);
  host->host_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::ASCIIToUTF16("alert('This should not crash.');"),
      base::NullCallback());

  ASSERT_NO_FATAL_FAILURE(CloseDialog());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, AlertQueue) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  const Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = ProcessManager::Get(browser()->profile())
                            ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(host);

  // Creates several dialogs at the same time.
  const size_t num_dialogs = 3;
  size_t call_count = 0;
  for (size_t i = 0; i != num_dialogs; ++i) {
    const std::string dialog_name = "Dialog #" + base::NumberToString(i) + ".";
    host->host_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("alert('" + dialog_name + "');"),
        base::BindOnce(&CheckAlertResult, dialog_name,
                       base::Unretained(&call_count)));
  }

  // Closes these dialogs.
  for (size_t i = 0; i != num_dialogs; ++i) {
    ASSERT_NO_FATAL_FAILURE(AcceptDialog());
  }

  // All dialogs must be closed now.
  app_modal::AppModalDialogQueue* queue =
      app_modal::AppModalDialogQueue::GetInstance();
  ASSERT_TRUE(queue);
  EXPECT_FALSE(queue->HasActiveDialog());
  EXPECT_EQ(0, queue->end() - queue->begin());
  while (call_count < num_dialogs)
    ASSERT_NO_FATAL_FAILURE(content::RunAllPendingInMessageLoop());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ConfirmQueue) {
  ASSERT_TRUE(RunExtensionTest("alert")) << message_;

  const Extension* extension = GetSingleLoadedExtension();
  ExtensionHost* host = ProcessManager::Get(browser()->profile())
                            ->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(host);

  // Creates several dialogs at the same time.
  const size_t num_accepted_dialogs = 3;
  const size_t num_cancelled_dialogs = 3;
  size_t call_count = 0;
  for (size_t i = 0; i != num_accepted_dialogs; ++i) {
    const std::string dialog_name =
        "Accepted dialog #" + base::NumberToString(i) + ".";
    host->host_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("confirm('" + dialog_name + "');"),
        base::BindOnce(&CheckConfirmResult, dialog_name, true,
                       base::Unretained(&call_count)));
  }
  for (size_t i = 0; i != num_cancelled_dialogs; ++i) {
    const std::string dialog_name =
        "Cancelled dialog #" + base::NumberToString(i) + ".";
    host->host_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("confirm('" + dialog_name + "');"),
        base::BindOnce(&CheckConfirmResult, dialog_name, false,
                       base::Unretained(&call_count)));
  }

  // Closes these dialogs.
  for (size_t i = 0; i != num_accepted_dialogs; ++i)
    ASSERT_NO_FATAL_FAILURE(AcceptDialog());
  for (size_t i = 0; i != num_cancelled_dialogs; ++i)
    ASSERT_NO_FATAL_FAILURE(CancelDialog());

  // All dialogs must be closed now.
  app_modal::AppModalDialogQueue* queue =
      app_modal::AppModalDialogQueue::GetInstance();
  ASSERT_TRUE(queue);
  EXPECT_FALSE(queue->HasActiveDialog());
  EXPECT_EQ(0, queue->end() - queue->begin());
  while (call_count < num_accepted_dialogs + num_cancelled_dialogs)
    ASSERT_NO_FATAL_FAILURE(content::RunAllPendingInMessageLoop());
}

}  // namespace extensions
