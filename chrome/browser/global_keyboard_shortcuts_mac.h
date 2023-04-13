// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
#define CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_

#include <stddef.h>

#include <vector>

#if defined(__OBJC__)
@class NSEvent;
#endif  // __OBJC__

namespace ui {
class Accelerator;
}

constexpr int NO_COMMAND = -1;

struct KeyboardShortcutData {
  bool command_key;
  bool shift_key;
  bool cntrl_key;
  bool opt_key;
  int vkey_code;  // Virtual Key code for the command.

  int chrome_command;  // The chrome command # to execute for this shortcut.
};

struct CommandForKeyEventResult {
  bool found() { return chrome_command != NO_COMMAND; }

  // The command to execute. NO_COMMAND if none was found.
  int chrome_command;

  // Whether the command was from a mapping in the main menu. Only relevant if
  // command != NO_COMMAND.
  bool from_main_menu;
};

#if defined(__OBJC__)

// macOS applications are supposed to put all keyEquivalents [hotkeys] in the
// menu bar. For legacy reasons, Chrome does not. There are around 30 hotkeys
// that are explicitly coded to virtual keycodes. This has the following
// downsides:
//  * There is no way for the user to configure or disable these keyEquivalents.
//  * This can cause keyEquivalent conflicts for non-US keyboard layouts with
//    different default keyEquivalents, see https://crbug.com/841299.
//
// This function first searches the menu bar for a matching keyEquivalent. If
// nothing is found, then it searches through the explicitly coded virtual
// keycodes not present in the NSMenu.
//
// Note: AppKit exposes symbolic hotkeys [e.g. cmd + `] not present in the
// NSMenu as well. The user can remap these to conflict with Chrome hotkeys.
// This function will return the Chrome hotkey, regardless of whether there's a
// conflicting symbolic hotkey.
CommandForKeyEventResult CommandForKeyEvent(NSEvent* event);

// For legacy reasons and compatibility with Safari, some commands [e.g. cmd +
// left arrow] are only allowed to fire if the firstResponder is a WebContents,
// and the WebContents has chosen not to handle the event.
int DelayedWebContentsCommandForKeyEvent(NSEvent* event);

// Whether the event goes through the performKeyEquivalent: path and is handled
// by CommandDispatcher.
bool EventUsesPerformKeyEquivalent(NSEvent* event);

#endif  // __OBJC__

// On macOS, most accelerators are defined in MainMenu.xib and are user
// configurable. Furthermore, their values and enabled state depends on the key
// window. Views code relies on a static mapping that is not dependent on the
// key window. Thus, we provide the default Mac accelerator for each CommandId,
// which is static. This may be inaccurate, but is at least sufficiently well
// defined for Views to use.
bool GetDefaultMacAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator);

// For testing purposes.
const std::vector<KeyboardShortcutData>& GetShortcutsNotPresentInMainMenu();

#endif  // CHROME_BROWSER_GLOBAL_KEYBOARD_SHORTCUTS_MAC_H_
