// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

using extensions::ScopedTestDialogAutoConfirm;

namespace {

scoped_refptr<const extensions::Extension> BuildTestExtension() {
  return extensions::ExtensionBuilder("foo").Build();
}

}  // namespace

typedef InProcessBrowserTest ExtensionInstallPromptBrowserTest;

// Test that ExtensionInstallPrompt aborts the install if the web contents which
// were passed to the ExtensionInstallPrompt constructor get destroyed.
// CrxInstaller takes in ExtensionInstallPrompt in the constructor and does a
// bunch of asynchronous processing prior to confirming the install. A user may
// close the current tab while this processing is taking place.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest,
                       TrackParentWebContentsDestruction) {
  AddBlankTabAndShow(browser());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  int web_contents_index = tab_strip_model->GetIndexOfWebContents(web_contents);
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(web_contents);
  tab_strip_model->CloseWebContentsAt(web_contents_index,
                                      TabStripModel::CLOSE_NONE);
  content::RunAllPendingInMessageLoop();

  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(
      helper.GetCallback(),
      extension.get(), nullptr,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ABORTED, helper.result());
}

// Test that ExtensionInstallPrompt aborts the install if the gfx::NativeWindow
// which is passed to the ExtensionInstallPrompt constructor is destroyed.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest,
                       TrackParentWindowDestruction) {
  // Create a second browser to prevent the app from exiting when the browser is
  // closed.
  CreateBrowser(browser()->profile());

  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(browser()->profile(),
                                browser()->window()->GetNativeWindow());
  browser()->window()->Close();
  content::RunAllPendingInMessageLoop();

  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(
      helper.GetCallback(),
      extension.get(), nullptr,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ABORTED, helper.result());
}

// Test that ExtensionInstallPrompt shows the dialog normally if no parent
// web contents or parent gfx::NativeWindow is passed to the
// ExtensionInstallPrompt constructor.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest, NoParent) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(browser()->profile(), NULL);
  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(
      helper.GetCallback(),
      extension.get(), nullptr,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
}
