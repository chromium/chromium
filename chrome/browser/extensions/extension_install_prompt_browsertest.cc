// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/browser_closed_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using extensions::InstallPromptData;
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

  ExtensionInstallPrompt prompt(
      web_contents,
      std::make_unique<InstallPromptData>(InstallPromptData::INSTALL_PROMPT));
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

// Test that ExtensionInstallPrompt aborts the install if the gfx::NativeWindow
// which is passed to the ExtensionInstallPrompt constructor is destroyed.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest,
                       TrackParentWindowDestruction) {
  // Create a second browser to prevent the app from exiting when the browser is
  // closed.
  CreateBrowserWindowWithType(BrowserWindowInterface::Type::TYPE_NORMAL);

  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(
      profile(), GetBrowserWindowInterface()->GetWindow()->GetNativeWindow(),
      std::make_unique<InstallPromptData>(InstallPromptData::INSTALL_PROMPT));
  BrowserClosedWaiter waiter(GetBrowserWindowInterface());
  GetBrowserWindowInterface()->GetWindow()->Close();
  waiter.Wait();

  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(helper.GetCallback(), extension.get(), nullptr,
                    ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ABORTED, helper.result());
}

// Test that ExtensionInstallPrompt shows the dialog normally if a parent
// gfx::NativeWindow is passed to the ExtensionInstallPrompt constructor.
// Regression test for https://crbug.com/552620245
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest, ParentWindow) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(
      profile(), GetBrowserWindowInterface()->GetWindow()->GetNativeWindow(),
      std::make_unique<InstallPromptData>(InstallPromptData::INSTALL_PROMPT));
  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(helper.GetCallback(), extension.get(), nullptr,
                    ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
}

// Test that ExtensionInstallPrompt shows the dialog normally if no parent
// web contents or parent gfx::NativeWindow is passed to the
// ExtensionInstallPrompt constructor.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest, NoParent) {
  scoped_refptr<const extensions::Extension> extension(BuildTestExtension());

  ScopedTestDialogAutoConfirm auto_confirm(ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionInstallPrompt prompt(
      profile(), gfx::NativeWindow(),
      std::make_unique<InstallPromptData>(InstallPromptData::INSTALL_PROMPT));
  base::RunLoop run_loop;
  ExtensionInstallPromptTestHelper helper(run_loop.QuitClosure());
  prompt.ShowDialog(
      helper.GetCallback(),
      extension.get(), nullptr,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
  run_loop.Run();
  EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
}
