// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/shortcut.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ttc {
namespace {

ShortcutStatus Validate(ui::KeyboardCode key_code, int modifiers) {
  return ValidateShortcut(ui::Accelerator(key_code, modifiers));
}

TEST(TtcShortcutTest, NeedsCtrlOrAlt) {
  EXPECT_EQ(ShortcutStatus::kInvalid, Validate(ui::VKEY_K, ui::EF_NONE));
  // Shift by itself types capital letters.
  EXPECT_EQ(ShortcutStatus::kInvalid, Validate(ui::VKEY_K, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_TAB, ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ShortcutStatus::kValid,
            Validate(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
}

TEST(TtcShortcutTest, RejectsKeysThePrefsCantName) {
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_F5, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_RETURN, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_CONTROL_DOWN));
  // AltGr isn't Alt.
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_ALTGR_DOWN));
}

TEST(TtcShortcutTest, RejectsReleasedKeys) {
  EXPECT_EQ(ShortcutStatus::kInvalid,
            ValidateShortcut(ui::Accelerator(
                ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                ui::Accelerator::KeyState::RELEASED)));
}

#if BUILDFLAG(IS_MAC)
TEST(TtcShortcutTest, RejectsOptionWithoutCommandOrControl) {
  // Option and Option+Shift type accented and special characters.
  EXPECT_EQ(ShortcutStatus::kInvalid, Validate(ui::VKEY_K, ui::EF_ALT_DOWN));
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_K, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
}

TEST(TtcShortcutTest, AllowsCommandAndOptionWithCommandOrControl) {
  EXPECT_EQ(ShortcutStatus::kValid,
            Validate(ui::VKEY_Y, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ShortcutStatus::kValid,
            Validate(ui::VKEY_Y, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN));
  EXPECT_EQ(ShortcutStatus::kValid,
            Validate(ui::VKEY_Y, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
}
#else
TEST(TtcShortcutTest, DefaultShortcutsAreValid) {
  EXPECT_EQ(ShortcutStatus::kValid,
            Validate(ui::VKEY_K, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
  EXPECT_EQ(ShortcutStatus::kValid,
            Validate(ui::VKEY_SPACE, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
}

TEST(TtcShortcutTest, RejectsModifiersNeededForTyping) {
  // Ctrl+Alt is AltGr on international keyboard layouts.
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_K, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN));
  // The operating system reserves the Meta, Search and Windows keys.
  EXPECT_EQ(ShortcutStatus::kInvalid,
            Validate(ui::VKEY_K, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN));
}
#endif  // BUILDFLAG(IS_MAC)

TEST(TtcShortcutTest, RejectsBrowserShortcuts) {
#if BUILDFLAG(IS_MAC)
  // Most of Chrome's shortcuts on macOS, such as Command+T, come from the main
  // menu, which unit tests don't create. These two don't: Command+Shift+M
  // opens the profile menu, and Command+Shift+C inspects an element.
  const ui::Accelerator browser_shortcuts[] = {
      ui::Accelerator(ui::VKEY_M, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN),
      ui::Accelerator(ui::VKEY_C, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN),
  };
#else
  // Ctrl+T opens a new tab, and Ctrl+W closes it.
  const ui::Accelerator browser_shortcuts[] = {
      ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN),
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN),
  };
#endif  // BUILDFLAG(IS_MAC)
  for (const ui::Accelerator& shortcut : browser_shortcuts) {
    SCOPED_TRACE(ui::Command::AcceleratorToString(shortcut));
    EXPECT_EQ(ShortcutStatus::kConflictsWithBrowserShortcut,
              ValidateShortcut(shortcut));
    // An accelerator made from a repeated or synthesized ui::KeyEvent keeps
    // those flags.
    EXPECT_EQ(
        ShortcutStatus::kConflictsWithBrowserShortcut,
        Validate(shortcut.key_code(), shortcut.modifiers() | ui::EF_IS_REPEAT));
    EXPECT_EQ(ShortcutStatus::kConflictsWithBrowserShortcut,
              Validate(shortcut.key_code(),
                       shortcut.modifiers() | ui::EF_IS_SYNTHESIZED));
  }
}

TEST(TtcShortcutTest, ParseShortcut) {
#if BUILDFLAG(IS_MAC)
  const ui::Accelerator command_shift_y(
      ui::VKEY_Y, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(command_shift_y, ParseShortcut("Command+Shift+Y"));
  // Modifiers can come in any order.
  EXPECT_EQ(command_shift_y, ParseShortcut("Shift+Command+Y"));

  // Option needs Command or Control.
  EXPECT_TRUE(ParseShortcut("Alt+Shift+K").IsEmpty());
  // Opens the profile menu.
  EXPECT_TRUE(ParseShortcut("Command+Shift+M").IsEmpty());
#else
  const ui::Accelerator alt_shift_k(ui::VKEY_K,
                                    ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(alt_shift_k, ParseShortcut("Alt+Shift+K"));
  // Modifiers can come in any order.
  EXPECT_EQ(alt_shift_k, ParseShortcut("Shift+Alt+K"));
  EXPECT_EQ(
      ui::Accelerator(ui::VKEY_SPACE, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      ParseShortcut("Alt+Shift+Space"));

  EXPECT_TRUE(ParseShortcut("Ctrl+Alt+K").IsEmpty());
  EXPECT_TRUE(ParseShortcut("Ctrl+T").IsEmpty());
#endif  // BUILDFLAG(IS_MAC)

  EXPECT_TRUE(ParseShortcut("").IsEmpty());
  EXPECT_TRUE(ParseShortcut("K").IsEmpty());
  EXPECT_TRUE(ParseShortcut("Shift+K").IsEmpty());
  EXPECT_TRUE(ParseShortcut("Alt+F5").IsEmpty());
  EXPECT_TRUE(ParseShortcut("Alt+Shift").IsEmpty());
  EXPECT_TRUE(ParseShortcut("not a shortcut").IsEmpty());
}

}  // namespace
}  // namespace ttc
