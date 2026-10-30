// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_banks_shortcut.h"

#include <optional>
#include <string>
#include <string_view>

#include "build/build_config.h"
#include "chrome/browser/context_hub/prefs.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace context_hub {
namespace {

// Returns a Ctrl+Shift+<letter> shortcut (Command+Shift+<letter> on macOS)
// that Chrome doesn't use, so the tests don't depend on a specific key staying
// free. Returns std::nullopt if Chrome uses all of them.
std::optional<ui::Accelerator> FreeShortcut() {
  for (int key = ui::VKEY_A; key <= ui::VKEY_Z; ++key) {
    const ui::Accelerator accelerator(
        static_cast<ui::KeyboardCode>(key),
        ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN);
    if (!IsChromeAccelerator(accelerator)) {
      return accelerator;
    }
  }
  return std::nullopt;
}

// Ctrl+Shift+M (Command+Shift+M on macOS), which opens the profile menu.
std::string ProfileMenuShortcut() {
  return ui::Command::AcceleratorToString(ui::Accelerator(
      ui::VKEY_M, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));
}

class MemoryBanksShortcutTest : public testing::Test {
 protected:
  MemoryBanksShortcutTest() {
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  MemoryBanksShortcutStatus Set(std::string_view shortcut) {
    return SetBrowseMemoryBanksShortcutInPrefs(pref_service_, shortcut);
  }

  std::string GetPref() const {
    return pref_service_.GetString(prefs::kContextHubBrowseMemoryBanksShortcut);
  }

  void SetPrefDirectly(const std::string& shortcut) {
    pref_service_.SetString(prefs::kContextHubBrowseMemoryBanksShortcut,
                            shortcut);
  }

  TestingPrefServiceSimple pref_service_;
};

TEST_F(MemoryBanksShortcutTest, NoShortcutByDefault) {
  EXPECT_EQ("", GetPref());
  EXPECT_TRUE(GetBrowseMemoryBanksShortcutFromPrefs(pref_service_).IsEmpty());
}

TEST_F(MemoryBanksShortcutTest, SetSavesValidShortcut) {
  const std::optional<ui::Accelerator> shortcut = FreeShortcut();
  if (!shortcut) {
    GTEST_SKIP() << "Chrome uses every Ctrl+Shift+<letter> shortcut.";
  }
  const std::string shortcut_string =
      ui::Command::AcceleratorToString(*shortcut);

  EXPECT_EQ(MemoryBanksShortcutStatus::kValid, Set(shortcut_string));
  EXPECT_EQ(shortcut_string, GetPref());
  EXPECT_EQ(*shortcut, GetBrowseMemoryBanksShortcutFromPrefs(pref_service_));
}

TEST_F(MemoryBanksShortcutTest, SetEmptyClearsShortcut) {
  const std::optional<ui::Accelerator> shortcut = FreeShortcut();
  if (!shortcut) {
    GTEST_SKIP() << "Chrome uses every Ctrl+Shift+<letter> shortcut.";
  }
  SetPrefDirectly(ui::Command::AcceleratorToString(*shortcut));

  EXPECT_EQ(MemoryBanksShortcutStatus::kValid, Set(""));
  EXPECT_EQ("", GetPref());
}

TEST_F(MemoryBanksShortcutTest, RejectsInvalidShortcuts) {
  // Not a shortcut.
  EXPECT_EQ(MemoryBanksShortcutStatus::kInvalid, Set("not a shortcut"));
  // No modifier, or only Shift.
  EXPECT_EQ(MemoryBanksShortcutStatus::kInvalid, Set("K"));
  EXPECT_EQ(MemoryBanksShortcutStatus::kInvalid, Set("Shift+K"));
  // Not a letter or a number.
  EXPECT_EQ(MemoryBanksShortcutStatus::kInvalid, Set("Ctrl+Comma"));
  EXPECT_EQ(MemoryBanksShortcutStatus::kInvalid, Set("Ctrl+Space"));

  EXPECT_EQ("", GetPref());
}

#if BUILDFLAG(IS_MAC)
TEST_F(MemoryBanksShortcutTest, RejectsOptionWithoutCommandOrControl) {
  EXPECT_EQ(MemoryBanksShortcutStatus::kDisallowedModifiers, Set("Alt+K"));
  EXPECT_EQ(MemoryBanksShortcutStatus::kDisallowedModifiers,
            Set("Alt+Shift+K"));
  EXPECT_EQ("", GetPref());
}
#else
TEST_F(MemoryBanksShortcutTest, RejectsCtrlAlt) {
  EXPECT_EQ(MemoryBanksShortcutStatus::kDisallowedModifiers, Set("Ctrl+Alt+K"));
  EXPECT_EQ("", GetPref());
}

// On macOS, Chrome's navigation shortcuts come from the main menu, which unit
// tests don't create.
TEST_F(MemoryBanksShortcutTest, RejectsBrowserNavigationShortcut) {
  // Ctrl+T opens a new tab.
  EXPECT_EQ(MemoryBanksShortcutStatus::kConflictsWithBrowserShortcut,
            Set("Ctrl+T"));
  EXPECT_EQ("", GetPref());
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(MemoryBanksShortcutTest, RejectsSearchKey) {
  EXPECT_EQ(MemoryBanksShortcutStatus::kDisallowedModifiers, Set("Search+K"));
  EXPECT_EQ("", GetPref());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(MemoryBanksShortcutTest, RejectsProfileMenuShortcut) {
  EXPECT_EQ(MemoryBanksShortcutStatus::kConflictsWithBrowserShortcut,
            Set(ProfileMenuShortcut()));
  EXPECT_EQ("", GetPref());
}

TEST_F(MemoryBanksShortcutTest, RejectedShortcutKeepsCurrentOne) {
  const std::optional<ui::Accelerator> shortcut = FreeShortcut();
  if (!shortcut) {
    GTEST_SKIP() << "Chrome uses every Ctrl+Shift+<letter> shortcut.";
  }
  const std::string current = ui::Command::AcceleratorToString(*shortcut);
  SetPrefDirectly(current);

  EXPECT_EQ(MemoryBanksShortcutStatus::kInvalid, Set("K"));
  EXPECT_EQ(MemoryBanksShortcutStatus::kConflictsWithBrowserShortcut,
            Set(ProfileMenuShortcut()));
  EXPECT_EQ(current, GetPref());
}

TEST_F(MemoryBanksShortcutTest, GetIgnoresInvalidStoredShortcut) {
  SetPrefDirectly("K");
  EXPECT_TRUE(GetBrowseMemoryBanksShortcutFromPrefs(pref_service_).IsEmpty());

  SetPrefDirectly("not a shortcut");
  EXPECT_TRUE(GetBrowseMemoryBanksShortcutFromPrefs(pref_service_).IsEmpty());
}

TEST_F(MemoryBanksShortcutTest, GetIgnoresConflictingStoredShortcut) {
  SetPrefDirectly(ProfileMenuShortcut());
  EXPECT_TRUE(GetBrowseMemoryBanksShortcutFromPrefs(pref_service_).IsEmpty());
}

TEST_F(MemoryBanksShortcutTest, SuggestedShortcutMatchesPlatform) {
#if BUILDFLAG(IS_MAC)
  // Control+Command+M.
  const ui::Accelerator expected(ui::VKEY_M,
                                 ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN);
#elif BUILDFLAG(IS_CHROMEOS)
  // Alt+M.
  const ui::Accelerator expected(ui::VKEY_M, ui::EF_ALT_DOWN);
#else
  // Alt+Shift+M.
  const ui::Accelerator expected(ui::VKEY_M,
                                 ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
#endif
  EXPECT_EQ(expected, GetSuggestedBrowseMemoryBanksShortcut());
}

TEST_F(MemoryBanksShortcutTest, SuggestedShortcutCanBeSaved) {
  EXPECT_EQ(MemoryBanksShortcutStatus::kValid,
            Set(ui::Command::AcceleratorToString(
                GetSuggestedBrowseMemoryBanksShortcut())));
}

}  // namespace
}  // namespace context_hub
