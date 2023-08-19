// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/global_keyboard_shortcuts_mac.h"

#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include "base/apple/foundation_util.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#include "chrome/browser/ui/ui_features.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#import "ui/base/cocoa/nsmenu_additions.h"
#import "ui/base/cocoa/nsmenuitem_additions.h"
#include "ui/base/ui_base_features.h"
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

int MenuCommandForKeyEvent(NSEvent* event) {
  NSMenuItem* item = [[NSApp mainMenu] cr_menuItemForKeyEquivalentEvent:event];

  if (!item)
    return NO_COMMAND;

  if ([item action] == @selector(commandDispatch:) && [item tag] > 0)
    return [item tag];

  // "Close window", "Quit", and other commands don't use the `commandDispatch:`
  // mechanism. Menu items that do not correspond to IDC_ constants need no
  // special treatment however, as they can't be reserved in
  // |BrowserCommandController::IsReservedCommandOrKey()| anyhow.
  SEL itemAction = [item action];

  if (itemAction == @selector(performClose:))
    return IDC_CLOSE_WINDOW;

  if (itemAction == @selector(terminate:))
    return IDC_EXIT;

  return NO_COMMAND;
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
  // clang-format off
  static base::NoDestructor<std::vector<KeyboardShortcutData>> keys({
    // cmd    shift  cntrl  option vkeycode               command
    // ---    -----  -----  ------ --------               -------
      {true,  false, false, false, kVK_LeftArrow,         IDC_BACK},
      {true,  false, false, false, kVK_RightArrow,        IDC_FORWARD},
  });
  // clang-format on
  return *keys;
}

CommandForKeyEventResult NoCommand() {
  return {NO_COMMAND, /*from_main_menu=*/false};
}

CommandForKeyEventResult MainMenuCommand(int cmd) {
  return {cmd, /*from_main_menu=*/true};
}

CommandForKeyEventResult ShortcutCommand(int cmd) {
  return {cmd, /*from_main_menu=*/false};
}

}  // namespace

// Returns a vector of hidden keyboard shortcuts (i.e. ones that arent present
// in the menus). Note that the hidden "Cmd =" shortcut is somehow enabled by
// the ui::VKEY_OEM_PLUS entry in accelerators_cocoa.mm.
const std::vector<KeyboardShortcutData>& GetShortcutsNotPresentInMainMenu() {
  static const base::NoDestructor<std::vector<KeyboardShortcutData>> keys([]() {
    // clang-format off
    std::vector<KeyboardShortcutData> keys({
    // cmd    shift  cntrl  option vkeycode               command
    // ---    -----  -----  ------ --------               -------
      {true,  true,  false, false, kVK_ANSI_RightBracket, IDC_SELECT_NEXT_TAB},
      {true,  true,  false, false, kVK_ANSI_LeftBracket,  IDC_SELECT_PREVIOUS_TAB},
      {false, false, true,  false, kVK_PageDown,          IDC_SELECT_NEXT_TAB},
      {false, false, true,  false, kVK_PageUp,            IDC_SELECT_PREVIOUS_TAB},
      {true,  false, false, true,  kVK_RightArrow,        IDC_SELECT_NEXT_TAB},
      {true,  false, false, true,  kVK_LeftArrow,         IDC_SELECT_PREVIOUS_TAB},
      {false, true,  true,  false, kVK_PageDown,          IDC_MOVE_TAB_NEXT},
      {false, true,  true,  false, kVK_PageUp,            IDC_MOVE_TAB_PREVIOUS},

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
      {true,  true,  false, true,  kVK_ANSI_A,            IDC_FOCUS_INACTIVE_POPUP_FOR_ACCESSIBILITY},
    });
    // clang-format on

    if (base::FeatureList::IsEnabled(features::kUIDebugTools)) {
      keys.push_back(
          {false, true, true, true, kVK_ANSI_T, IDC_DEBUG_TOGGLE_TABLET_MODE});
      keys.push_back(
          {false, true, true, true, kVK_ANSI_V, IDC_DEBUG_PRINT_VIEW_TREE});
      keys.push_back({false, true, true, true, kVK_ANSI_M,
                      IDC_DEBUG_PRINT_VIEW_TREE_DETAILS});
    }
    return keys;
  }());
  return *keys;
}

const std::vector<NSMenuItem*>& GetMenuItemsNotPresentInMainMenu() {
  static base::NoDestructor<std::vector<NSMenuItem*>> menu_items([]() {
    std::vector<NSMenuItem*> menu_items;
    for (const auto& shortcut : GetShortcutsNotPresentInMainMenu()) {
      ui::Accelerator accelerator = AcceleratorFromShortcut(shortcut);
      KeyEquivalentAndModifierMask* equivalent =
          ui::GetKeyEquivalentAndModifierMaskFromAccelerator(accelerator);

      // Intentionally leaked!
      NSMenuItem* item =
          [[NSMenuItem alloc] initWithTitle:@""
                                     action:nullptr
                              keyEquivalent:equivalent.keyEquivalent];
      item.keyEquivalentModifierMask = equivalent.modifierMask;

      // We store the command in the tag.
      item.tag = shortcut.chrome_command;
      menu_items.push_back(item);
    }
    return menu_items;
  }());
  return *menu_items;
}

CommandForKeyEventResult CommandForKeyEvent(NSEvent* event) {
  DCHECK(event);
  if ([event type] != NSEventTypeKeyDown)
    return NoCommand();

  int cmdNum = MenuCommandForKeyEvent(event);
  if (cmdNum != NO_COMMAND)
    return MainMenuCommand(cmdNum);

  // Scan through keycodes and see if it corresponds to one of the non-menu
  // shortcuts.
  for (NSMenuItem* menu_item : GetMenuItemsNotPresentInMainMenu()) {
    if ([menu_item cr_firesForKeyEquivalentEvent:event])
      return ShortcutCommand(menu_item.tag);
  }

  return NoCommand();
}

int DelayedWebContentsCommandForKeyEvent(NSEvent* event) {
  DCHECK(event);
  if ([event type] != NSEventTypeKeyDown)
    return NO_COMMAND;

  // Look in secondary keyboard shortcuts.
  NSUInteger modifiers = [event modifierFlags];
  const bool cmdKey = (modifiers & NSEventModifierFlagCommand) != 0;
  const bool shiftKey = (modifiers & NSEventModifierFlagShift) != 0;
  const bool cntrlKey = (modifiers & NSEventModifierFlagControl) != 0;
  const bool optKey = (modifiers & NSEventModifierFlagOption) != 0;
  const int keyCode = [event keyCode];

  // Scan through keycodes and see if it corresponds to one of the non-menu
  // shortcuts.
  for (const auto& shortcut : GetDelayedShortcutsNotPresentInMainMenu()) {
    if (MatchesEventForKeyboardShortcut(shortcut, cmdKey, shiftKey, cntrlKey,
                                        optKey, keyCode)) {
      return shortcut.chrome_command;
    }
  }

  return NO_COMMAND;
}

// AppKit sends an event via performKeyEquivalent: if it has at least one of the
// command or control modifiers, and is an NSEventTypeKeyDown event.
// CommandDispatcher supplements this by also sending event with the option
// modifier to performKeyEquivalent:.
bool EventUsesPerformKeyEquivalent(NSEvent* event) {
  return ([event modifierFlags] & ui::cocoa::ModifierMaskForKeyEvent(event)) !=
         0;
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
