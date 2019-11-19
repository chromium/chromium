// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace {

// Returns a ui::Accelerator given a KeyboardShortcutData.
ui::Accelerator AcceleratorFromShortcut(const KeyboardShortcutData& shortcut) {
  int modifiers = 0;
  if (shortcut.command_key)
    modifiers |= ui::EF_COMMAND_DOWN;
  if (shortcut.shift_key)
    modifiers |= ui::EF_SHIFT_DOWN;
  if (shortcut.cntrl_key)
    modifiers |= ui::EF_CONTROL_DOWN;
  if (shortcut.opt_key)
    modifiers |= ui::EF_ALT_DOWN;

  return ui::Accelerator(ui::KeyboardCodeFromKeyCode(shortcut.vkey_code),
                         modifiers);
}

// Returns the menu item associated with |key| in |menu|, or nil if not found.
NSMenuItem* FindMenuItem(NSEvent* key, NSMenu* menu) {
  NSMenuItem* result = nil;

  for (NSMenuItem* item in [menu itemArray]) {
    NSMenu* submenu = [item submenu];
    if (submenu) {
      if (submenu != [NSApp servicesMenu])
        result = FindMenuItem(key, submenu);
    } else if ([item cr_firesForKeyEvent:key]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
}

int MenuCommandForKeyEvent(NSEvent* event) {
  if ([event type] != NSKeyDown)
    return -1;

  // We avoid calling -[NSMenuDelegate menuNeedsUpdate:] on each submenu's
  // delegate as that can be slow. Instead, we update the relevant NSMenuItems
  // if [NSApp delegate] is an instance of AppController. See
  // https://crbug.com/851260#c4.
  [base::mac::ObjCCast<AppController>([NSApp delegate])
      updateMenuItemKeyEquivalents];

  // Then call -[NSMenu update], which will validate every user interface item.
  [[NSApp mainMenu] update];

  NSMenuItem* item = FindMenuItem(event, [NSApp mainMenu]);

  if (!item)
    return -1;

  if ([item action] == @selector(commandDispatch:) && [item tag] > 0)
    return [item tag];

  // "Close window" doesn't use the |commandDispatch:| mechanism. Menu items
  // that do not correspond to IDC_ constants need no special treatment however,
  // as they can't be blacklisted in
  // |BrowserCommandController::IsReservedCommandOrKey()| anyhow.
  if ([item action] == @selector(performClose:))
    return IDC_CLOSE_WINDOW;

  // "Exit" doesn't use the |commandDispatch:| mechanism either.
  if ([item action] == @selector(terminate:))
    return IDC_EXIT;

  return -1;
}

bool MatchesEventForKeyboardShortcut(const KeyboardShortcutData& shortcut,
                                     bool command_key,
                                     bool shift_key,
                                     bool cntrl_key,
                                     bool opt_key,
                                     int vkey_code) {
  return shortcut.command_key == command_key &&
         shortcut.shift_key == shift_key && shortcut.cntrl_key == cntrl_key &&
         shortcut.opt_key == opt_key && shortcut.vkey_code == vkey_code;
}

const std::vector<KeyboardShortcutData>&
GetDelayedShortcutsNotPresentInMainMenu() {
  static base::NoDestructor<std::vector<KeyboardShortcutData>> keys({
      // cmd   shift  cntrl  option vkeycode               command
      //---   -----  -----  ------ --------               -------
      {true, false, false, false, kVK_LeftArrow, IDC_BACK},
      {true, false, false, false, kVK_RightArrow, IDC_FORWARD},
  });
  return *keys;
}

CommandForKeyEventResult NoCommand() {
  return {-1, /*from_main_menu=*/false};
}

CommandForKeyEventResult MainMenuCommand(int cmd) {
  return {cmd, /*from_main_menu=*/true};
}

CommandForKeyEventResult ShortcutCommand(int cmd) {
  return {cmd, /*from_main_menu=*/false};
}

}  // namespace

const std::vector<KeyboardShortcutData>& GetShortcutsNotPresentInMainMenu() {
  // clang-format off
  static base::NoDestructor<std::vector<KeyboardShortcutData>> keys({
  // cmd    shift  cntrl  option vkeycode               command
  // ---    -----  -----  ------ --------               -------
    {true,  true,  false, false, kVK_ANSI_RightBracket, IDC_SELECT_NEXT_TAB},
    {true,  true,  false, false, kVK_ANSI_LeftBracket,  IDC_SELECT_PREVIOUS_TAB},
    {false, false, true,  false, kVK_PageDown,          IDC_SELECT_NEXT_TAB},
    {false, false, true,  false, kVK_PageUp,            IDC_SELECT_PREVIOUS_TAB},
    {true,  false, false, true,  kVK_RightArrow,        IDC_SELECT_NEXT_TAB},
    {true,  false, false, true,  kVK_LeftArrow,         IDC_SELECT_PREVIOUS_TAB},

    // Cmd-0..8 select the nth tab, with cmd-9 being "last tab".
    {true,  false, false, false, kVK_ANSI_1,            IDC_SELECT_TAB_0},
    {true,  false, false, false, kVK_ANSI_Keypad1,      IDC_SELECT_TAB_0},
    {true,  false, false, false, kVK_ANSI_2,            IDC_SELECT_TAB_1},
    {true,  false, false, false, kVK_ANSI_Keypad2,      IDC_SELECT_TAB_1},
    {true,  false, false, false, kVK_ANSI_3,            IDC_SELECT_TAB_2},
    {true,  false, false, false, kVK_ANSI_Keypad3,      IDC_SELECT_TAB_2},
    {true,  false, false, false, kVK_ANSI_4,            IDC_SELECT_TAB_3},
    {true,  false, false, false, kVK_ANSI_Keypad4,      IDC_SELECT_TAB_3},
    {true,  false, false, false, kVK_ANSI_5,            IDC_SELECT_TAB_4},
    {true,  false, false, false, kVK_ANSI_Keypad5,      IDC_SELECT_TAB_4},
    {true,  false, false, false, kVK_ANSI_6,            IDC_SELECT_TAB_5},
    {true,  false, false, false, kVK_ANSI_Keypad6,      IDC_SELECT_TAB_5},
    {true,  false, false, false, kVK_ANSI_7,            IDC_SELECT_TAB_6},
    {true,  false, false, false, kVK_ANSI_Keypad7,      IDC_SELECT_TAB_6},
    {true,  false, false, false, kVK_ANSI_8,            IDC_SELECT_TAB_7},
    {true,  false, false, false, kVK_ANSI_Keypad8,      IDC_SELECT_TAB_7},
    {true,  false, false, false, kVK_ANSI_9,            IDC_SELECT_LAST_TAB},
    {true,  false, false, false, kVK_ANSI_Keypad9,      IDC_SELECT_LAST_TAB},
    {true,  true,  false, false, kVK_ANSI_M,            IDC_SHOW_AVATAR_MENU},
    {true,  false, false, true,  kVK_ANSI_L,            IDC_SHOW_DOWNLOADS},
    {true,  true,  false, false, kVK_ANSI_C,            IDC_DEV_TOOLS_INSPECT},
    {true,  false, false, true,  kVK_ANSI_C,            IDC_DEV_TOOLS_INSPECT},

    {true,  false, false, true,  kVK_DownArrow,         IDC_FOCUS_NEXT_PANE},
    {true,  false, false, true,  kVK_UpArrow,           IDC_FOCUS_PREVIOUS_PANE},
  });
  // clang-format on
  return *keys;
}

const std::vector<NSMenuItem*>& GetMenuItemsNotPresentInMainMenu() {
  static base::NoDestructor<std::vector<NSMenuItem*>> menu_items;
  if (menu_items->empty()) {
    for (const auto& shortcut : GetShortcutsNotPresentInMainMenu()) {
      ui::Accelerator accelerator = AcceleratorFromShortcut(shortcut);
      NSString* key_equivalent = nil;
      NSUInteger modifier_mask = 0;
      ui::GetKeyEquivalentAndModifierMaskFromAccelerator(
          accelerator, &key_equivalent, &modifier_mask);

      // Intentionally leaked!
      NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:@""
                                                    action:NULL
                                             keyEquivalent:key_equivalent];
      item.keyEquivalentModifierMask = modifier_mask;

      // We store the command in the tag.
      item.tag = shortcut.chrome_command;
      menu_items->push_back(item);
    }
  }
  return *menu_items;
}

CommandForKeyEventResult CommandForKeyEvent(NSEvent* event) {
  DCHECK(event);
  if ([event type] != NSKeyDown)
    return NoCommand();

  int cmdNum = MenuCommandForKeyEvent(event);
  if (cmdNum != -1)
    return MainMenuCommand(cmdNum);

  // Scan through keycodes and see if it corresponds to one of the non-menu
  // shortcuts.
  for (NSMenuItem* menu_item : GetMenuItemsNotPresentInMainMenu()) {
    if ([menu_item cr_firesForKeyEvent:event])
      return ShortcutCommand(menu_item.tag);
  }

  return NoCommand();
}

int DelayedWebContentsCommandForKeyEvent(NSEvent* event) {
  DCHECK(event);
  if ([event type] != NSKeyDown)
    return -1;

  // Look in secondary keyboard shortcuts.
  NSUInteger modifiers = [event modifierFlags];
  const bool cmdKey = (modifiers & NSCommandKeyMask) != 0;
  const bool shiftKey = (modifiers & NSShiftKeyMask) != 0;
  const bool cntrlKey = (modifiers & NSControlKeyMask) != 0;
  const bool optKey = (modifiers & NSAlternateKeyMask) != 0;
  const int keyCode = [event keyCode];

  // Scan through keycodes and see if it corresponds to one of the non-menu
  // shortcuts.
  for (const auto& shortcut : GetDelayedShortcutsNotPresentInMainMenu()) {
    if (MatchesEventForKeyboardShortcut(shortcut, cmdKey, shiftKey, cntrlKey,
                                        optKey, keyCode)) {
      return shortcut.chrome_command;
    }
  }

  return -1;
}

// AppKit sends an event via performKeyEquivalent: if it has at least one of the
// command or control modifiers, and is an NSKeyDown event. CommandDispatcher
// supplements this by also sending event with the option modifier to
// performKeyEquivalent:.
bool EventUsesPerformKeyEquivalent(NSEvent* event) {
  NSUInteger modifiers = [event modifierFlags];
  if ((modifiers & (NSEventModifierFlagCommand | NSEventModifierFlagControl |
                    NSEventModifierFlagOption)) == 0) {
    return false;
  }
  return [event type] == NSKeyDown;
}

bool GetDefaultMacAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator) {
  // See if it corresponds to one of the non-menu shortcuts.
  for (const auto& shortcut : GetShortcutsNotPresentInMainMenu()) {
    if (shortcut.chrome_command == command_id) {
      *accelerator = AcceleratorFromShortcut(shortcut);
      return true;
    }
  }

  // See if it corresponds to one of the default NSMenu keyEquivalents.
  const ui::Accelerator* default_nsmenu_equivalent =
      AcceleratorsCocoa::GetInstance()->GetAcceleratorForCommand(command_id);
  if (default_nsmenu_equivalent)
    *accelerator = *default_nsmenu_equivalent;
  return default_nsmenu_equivalent != nullptr;
}
