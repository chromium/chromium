// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "ui/gfx/native_widget_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using extensions::ScopedTestDialogAutoConfirm;

namespace {

scoped_refptr<const extensions::Extension> BuildTestExtension() {
  return extensions::ExtensionBuilder("foo").Build();
}

}  // namespace

using ExtensionInstallPromptBrowserTest = extensions::ExtensionBrowserTest;

// Test that ExtensionInstallPrompt aborts the install if the web contents which
// were passed to the ExtensionInstallPrompt constructor get destroyed.
// CrxInstaller takes in ExtensionInstallPrompt in the constructor and does a
// bunch of asynchronous processing prior to confirming the install. A user may
// close the current tab while this processing is taking place.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest,
                       TrackParentWebContentsDestruction) {
  NavigateToURLInNewTab(GURL("about:blank"));
  content::WebContents* web_contents = GetActiveWebContents();
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(web_contents);
  CloseTabForWebContents(web_contents);
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

#if !BUILDFLAG(IS_ANDROID)
// Test that ExtensionInstallPrompt aborts the install if the gfx::NativeWindow
// which is passed to the ExtensionInstallPrompt constructor is destroyed.
// TODO(crbug.com/397754565): Port to desktop Android when the install UI is
// supported.
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
#endif  // !BUILDFLAG(IS_ANDROID)

// Test that ExtensionInstallPrompt shows the dialog normally if no parent
// web contents or parent gfx::NativeWindow is passed to the
// ExtensionInstallPrompt constructor.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest, NoParent) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(profile(), gfx::NativeWindow());
  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(
      helper.GetCallback(),
      extension.get(), nullptr,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
}
