// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
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

// A file chooser requested by a tab that is not the active tab must not be
// shown. The dialog is parented to the browser window rather than to the tab,
// so showing it would place it on top of whichever page the user is actually
// looking at.
IN_PROC_BROWSER_TEST_F(FileSelectHelperBrowserTest, NoDialogForBackgroundTab) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/file_input.html")));
  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open a second tab in the foreground, so the page above is no longer the
  // active tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_NE(background_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  ui::FakeSelectFileDialog::Factory* factory =
      ui::FakeSelectFileDialog::RegisterFactory();
  bool dialog_opened = false;
  factory->SetOpenCallback(base::BindRepeating(
      [](bool* opened) { *opened = true; }, &dialog_opened));

  // Request a file chooser from the background tab. ExecJs supplies a user
  // gesture, so this reaches the browser exactly as a real request would.
  EXPECT_TRUE(ExecJs(background_contents,
                     "document.getElementById('fileinput').click();"));

  // Flush the thread pool hop that RunFileChooser() posts, plus the reply back
  // to the UI thread, so that the dialog would have been shown by now.
  content::RunAllTasksUntilIdle();

  EXPECT_FALSE(dialog_opened);
  EXPECT_FALSE(factory->GetLastDialog());
}

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
