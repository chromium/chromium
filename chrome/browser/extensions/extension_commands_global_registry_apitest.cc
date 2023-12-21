// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/result_catcher.h"
#include "ui/base/base_window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"

namespace extensions {

using GlobalCommandsApiTest = ExtensionApiTest;

// Test the basics of global commands and make sure they work when Chrome
// doesn't have focus. Also test that non-global commands are not treated as
// global and that keys beyond Ctrl+Shift+[0..9] cannot be auto-assigned by an
// extension.
//
// Doesn't work in CrOS builds, http://crbug.com/619784
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_GlobalCommand DISABLED_GlobalCommand
#else
#define MAYBE_GlobalCommand GlobalCommand
#endif
IN_PROC_BROWSER_TEST_F(GlobalCommandsApiTest, MAYBE_GlobalCommand) {
  // Load the extension in the non-incognito browser.
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/global")) << message_;
  ASSERT_TRUE(catcher.GetNextResult());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  // Our infrastructure for sending keys expects a browser to send them to, but
  // to properly test global shortcuts you need to send them to another target.
  // So, create an incognito browser to use as a target to send the shortcuts
  // to. It will ignore all of them and allow us test whether the global
  // shortcut really is global in nature and also that the non-global shortcut
  // is non-global.
  Browser* incognito_browser = CreateIncognitoBrowser();

  // Try to activate the non-global shortcut (Ctrl+Shift+1) and the
  // non-assignable shortcut (Ctrl+Shift+A) by sending the keystrokes to the
  // incognito browser. Both shortcuts should have no effect (extension is not
  // loaded there).
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_1, true, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_A, true, true, false, false));

  // Activate the shortcut (Ctrl+Shift+8). This should have an effect.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      incognito_browser, ui::VKEY_8, true, true, false, false));
#elif BUILDFLAG(IS_MAC)
  // As of macOS 10.14 (i.e. every supported macOS release), global event
  // injection requires user permission, which is something that can't happen in
  // the context of an automated test. Therefore, skip.
  GTEST_SKIP() << "macOS does not allow global event injection";
#endif

  // If this fails, it might be because the global shortcut failed to work,
  // but it might also be because the non-global shortcuts unexpectedly
  // worked.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if BUILDFLAG(IS_WIN)
// Feature only fully implemented on Windows, other platforms coming.
// TODO(smus): On mac, SendKeyPress must first support media keys.
#define MAYBE_GlobalDuplicatedMediaKey GlobalDuplicatedMediaKey
#else
#define MAYBE_GlobalDuplicatedMediaKey DISABLED_GlobalDuplicatedMediaKey
#endif

IN_PROC_BROWSER_TEST_F(GlobalCommandsApiTest, MAYBE_GlobalDuplicatedMediaKey) {
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/global_media_keys_0")) << message_;
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(RunExtensionTest("keybinding/global_media_keys_1")) << message_;
  ASSERT_TRUE(catcher.GetNextResult());

  Browser* incognito_browser = CreateIncognitoBrowser();  // Ditto.
  BrowserExtensionWindowController* controller =
      incognito_browser->extension_window_controller();

  ui_controls::SendKeyPress(controller->window()->GetNativeWindow(),
                            ui::VKEY_MEDIA_NEXT_TRACK,
                            false,
                            false,
                            false,
                            false);

  // We should get two success results.
  ASSERT_TRUE(catcher.GetNextResult());
  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
