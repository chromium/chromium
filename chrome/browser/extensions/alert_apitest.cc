// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace {

void GetNextDialog(javascript_dialogs::AppModalDialogView** view) {
  DCHECK(view);
  *view = nullptr;
  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  *view = dialog->view();
  ASSERT_TRUE(*view);
}

void CloseDialog() {
  javascript_dialogs::AppModalDialogView* dialog = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetNextDialog(&dialog));
  dialog->CloseAppModalDialog();
}

void AcceptDialog() {
  javascript_dialogs::AppModalDialogView* dialog = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetNextDialog(&dialog));
  dialog->AcceptAppModalDialog();
}

void CancelDialog() {
  javascript_dialogs::AppModalDialogView* dialog = nullptr;
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
  host->host_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      u"alert('This should not crash.');", base::NullCallback(),
      content::ISOLATED_WORLD_ID_GLOBAL);

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
    host->host_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"alert('" + base::ASCIIToUTF16(dialog_name) + u"');",
        base::BindOnce(&CheckAlertResult, dialog_name,
                       base::Unretained(&call_count)),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }

  // Closes these dialogs.
  for (size_t i = 0; i != num_dialogs; ++i) {
    ASSERT_NO_FATAL_FAILURE(AcceptDialog());
  }

  // All dialogs must be closed now.
  javascript_dialogs::AppModalDialogQueue* queue =
      javascript_dialogs::AppModalDialogQueue::GetInstance();
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
    host->host_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"confirm('" + base::ASCIIToUTF16(dialog_name) + u"');",
        base::BindOnce(&CheckConfirmResult, dialog_name, true,
                       base::Unretained(&call_count)),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }
  for (size_t i = 0; i != num_cancelled_dialogs; ++i) {
    const std::string dialog_name =
        "Cancelled dialog #" + base::NumberToString(i) + ".";
    host->host_contents()->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"confirm('" + base::ASCIIToUTF16(dialog_name) + u"');",
        base::BindOnce(&CheckConfirmResult, dialog_name, false,
                       base::Unretained(&call_count)),
        content::ISOLATED_WORLD_ID_GLOBAL);
  }

  // Closes these dialogs.
  for (size_t i = 0; i != num_accepted_dialogs; ++i)
    ASSERT_NO_FATAL_FAILURE(AcceptDialog());
  for (size_t i = 0; i != num_cancelled_dialogs; ++i)
    ASSERT_NO_FATAL_FAILURE(CancelDialog());

  // All dialogs must be closed now.
  javascript_dialogs::AppModalDialogQueue* queue =
      javascript_dialogs::AppModalDialogQueue::GetInstance();
  ASSERT_TRUE(queue);
  EXPECT_FALSE(queue->HasActiveDialog());
  EXPECT_EQ(0, queue->end() - queue->begin());
  while (call_count < num_accepted_dialogs + num_cancelled_dialogs)
    ASSERT_NO_FATAL_FAILURE(content::RunAllPendingInMessageLoop());
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DialogTitleShowsExtensionNameWithPrefix) {
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_path));
  base::FilePath extension_path =
      test_data_path.AppendASCII("extensions").AppendASCII("simple_with_popup");
  const Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);
  const GURL extension_url = extension->GetResourceURL("popup.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), extension_url));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify the title that would be used for a dialog spawned by extension.
  javascript_dialogs::AppModalDialogManager* dialog_manager =
      javascript_dialogs::AppModalDialogManager::GetInstance();
  EXPECT_EQ(u"The extension My First Extension says",
            dialog_manager->GetTitle(tab, extension->origin()));
}

}  // namespace extensions
