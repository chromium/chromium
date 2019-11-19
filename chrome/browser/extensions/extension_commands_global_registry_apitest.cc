// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/test/result_catcher.h"
#include "ui/base/base_window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/event_constants.h"

#if defined(USE_X11)
#include "ui/aura/window.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_observer_x11.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"
#endif

#if defined(OS_MACOSX)
#include <Carbon/Carbon.h>
#include "base/mac/mac_util.h"
#endif

namespace extensions {

typedef ExtensionApiTest GlobalCommandsApiTest;

#if defined(USE_X11)
// Send a simulated key press and release event, where |control|, |shift| or
// |alt| indicates whether the key is struck with corresponding modifier.
void SendNativeKeyEventToXDisplay(ui::KeyboardCode key,
                                  bool control,
                                  bool shift,
                                  bool alt) {
  Display* display = gfx::GetXDisplay();
  KeyCode ctrl_key_code = XKeysymToKeycode(display, XK_Control_L);
  KeyCode shift_key_code = XKeysymToKeycode(display, XK_Shift_L);
  KeyCode alt_key_code = XKeysymToKeycode(display, XK_Alt_L);

  // Release modifiers first of all to make sure this function can work as
  // expected. For example, when |control| is false, but the status of Ctrl key
  // is down, we will generate a keyboard event with unwanted Ctrl key.
  XTestFakeKeyEvent(display, ctrl_key_code, x11::False, x11::CurrentTime);
  XTestFakeKeyEvent(display, shift_key_code, x11::False, x11::CurrentTime);
  XTestFakeKeyEvent(display, alt_key_code, x11::False, x11::CurrentTime);

  typedef std::vector<KeyCode> KeyCodes;
  KeyCodes key_codes;
  if (control)
    key_codes.push_back(ctrl_key_code);
  if (shift)
    key_codes.push_back(shift_key_code);
  if (alt)
    key_codes.push_back(alt_key_code);

  key_codes.push_back(XKeysymToKeycode(display,
                                       XKeysymForWindowsKeyCode(key, false)));

  // Simulate the keys being pressed.
  for (auto it = key_codes.begin(); it != key_codes.end(); it++)
    XTestFakeKeyEvent(display, *it, x11::True, x11::CurrentTime);

  // Simulate the keys being released.
  for (auto it = key_codes.begin(); it != key_codes.end(); it++)
    XTestFakeKeyEvent(display, *it, x11::False, x11::CurrentTime);

  XFlush(display);
}
#endif  // defined(USE_X11)

// Test the basics of global commands and make sure they work when Chrome
// doesn't have focus. Also test that non-global commands are not treated as
// global and that keys beyond Ctrl+Shift+[0..9] cannot be auto-assigned by an
// extension.
//
// Doesn't work in CrOS builds, http://crbug.com/619784
#if defined(OS_CHROMEOS)
#define MAYBE_GlobalCommand DISABLED_GlobalCommand
#else
#define MAYBE_GlobalCommand GlobalCommand
#endif
IN_PROC_BROWSER_TEST_F(GlobalCommandsApiTest, MAYBE_GlobalCommand) {
  // Load the extension in the non-incognito browser.
  ResultCatcher catcher;
  ASSERT_TRUE(RunExtensionTest("keybinding/global")) << message_;
  ASSERT_TRUE(catcher.GetNextResult());

#if defined(OS_WIN) || defined(OS_CHROMEOS)
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
#elif defined(USE_X11)
  // Create an incognito browser to capture the focus.
  Browser* incognito_browser = CreateIncognitoBrowser();

  views::DesktopWindowTreeHostX11* host =
      static_cast<views::DesktopWindowTreeHostX11*>(
          incognito_browser->window()->GetNativeWindow()->GetHost());

  class GlobalCommandTreeHostObserver
      : public views::DesktopWindowTreeHostObserverX11 {
   public:
    void OnWindowMapped(unsigned long xid) override {
      // On Linux, our infrastructure for sending keys just synthesize keyboard
      // event and send them directly to the specified window, without notifying
      // the X root window. It didn't work while testing global shortcut because
      // the stuff of global shortcut on Linux need to be notified when KeyPress
      // event is happening on X root window. So we simulate the keyboard input
      // here.
      SendNativeKeyEventToXDisplay(ui::VKEY_1, true, true, false);
      SendNativeKeyEventToXDisplay(ui::VKEY_A, true, true, false);
      SendNativeKeyEventToXDisplay(ui::VKEY_8, true, true, false);
    }

    void OnWindowUnmapped(unsigned long xid) override {}
  } observer;

  // The observer sends the commands after window mapping
  host->AddObserver(&observer);

#elif defined(OS_MACOSX)
  // ui_test_utils::SendGlobalKeyEventsAndWait() hangs the test on macOS 10.14 -
  // https://crbug.com/904403
  if (base::mac::IsAtLeastOS10_14())
    return;

  // Create an incognito browser to capture the focus.
  Browser* incognito_browser = CreateIncognitoBrowser();
  // Activate Chrome.app so that events are seen on [NSApplication sendEvent:].
  // This is not necessary to detect these system events in release code, but is
  // a necessary trade-off to ensure all parts of the generated events have been
  // consumed. Without that, the test can leave the system in a state with the
  // Shift key permanently pressed and cause other tests to fail. But since it
  // is an incognito window that has focus, we still get good coverage of the
  // global hotkey logic.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(incognito_browser));

  // Send some native mac key events.
  ui_test_utils::SendGlobalKeyEventsAndWait(
      kVK_ANSI_1, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  ui_test_utils::SendGlobalKeyEventsAndWait(
      kVK_ANSI_A, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  ui_test_utils::SendGlobalKeyEventsAndWait(
      kVK_ANSI_8, ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
#endif

  // If this fails, it might be because the global shortcut failed to work,
  // but it might also be because the non-global shortcuts unexpectedly
  // worked.
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
#if defined(USE_X11)
  host->RemoveObserver(&observer);
#endif
}

#if defined(OS_WIN)
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
