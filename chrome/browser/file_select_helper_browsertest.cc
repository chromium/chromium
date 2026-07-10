// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/shell_dialogs/fake_select_file_dialog.h"

class FileSelectHelperBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }
};

// Verifies that subsequent file picker dialogs can be opened. This was added
// to prevent regressions like https://crrev.com/c/7810279, where
// FileSelectionCanceled was not called and thus the file picker dialog would
// not open more than once.
IN_PROC_BROWSER_TEST_F(FileSelectHelperBrowserTest, MultipleFilePickers) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/file_input.html")));

  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();

  // Open the file picker dialog
  bool dialog_opened_1 = false;
  factory->SetOpenCallback(base::BindRepeating(
      [](bool* opened) { *opened = true; }, &dialog_opened_1));
  EXPECT_TRUE(
      ExecJs(web_contents, "document.getElementById('fileinput').click();"));
  ASSERT_TRUE(base::test::RunUntil([&]() { return dialog_opened_1; }));

  // Cancel it
  ui::FakeSelectFileDialog* dialog_1 = factory->GetLastDialog();
  ASSERT_TRUE(dialog_1);
  dialog_1->CallFileSelectionCanceled();

  // Open a second dialog
  bool dialog_opened_2 = false;
  factory->SetOpenCallback(base::BindRepeating(
      [](bool* opened) { *opened = true; }, &dialog_opened_2));
  EXPECT_TRUE(
      ExecJs(web_contents, "document.getElementById('fileinput').click();"));
  ASSERT_TRUE(base::test::RunUntil([&]() { return dialog_opened_2; }));

  // Retrieve the second dialog and cancel it too.
  ui::FakeSelectFileDialog* dialog_2 = factory->GetLastDialog();
  ASSERT_TRUE(dialog_2);
  dialog_2->CallFileSelectionCanceled();
}
