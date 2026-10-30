// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/memory_banks_shortcut.h"

#include <string_view>

#include "build/build_config.h"
#include "chrome/browser/context_hub/prefs.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace context_hub {

namespace {

bool IsSupportedKey(ui::KeyboardCode key) {
  return (key >= ui::VKEY_A && key <= ui::VKEY_Z) ||
         (key >= ui::VKEY_0 && key <= ui::VKEY_9);
}

bool HasDisallowedModifiers(const ui::Accelerator& accelerator) {
#if BUILDFLAG(IS_MAC)
  // Option (and Shift+Option) with neither Command nor Control is used to type
  // accented and special characters.
  return accelerator.IsAltDown() && !accelerator.IsCmdDown() &&
         !accelerator.IsCtrlDown();
#else
  // Ctrl+Alt is the same as AltGr, which international keyboard layouts need
  // to type characters. The Windows key and the ChromeOS Search key are
  // reserved by the operating system.
  return (accelerator.IsCtrlDown() && accelerator.IsAltDown()) ||
         accelerator.IsCmdDown();
#endif
}

// Returns whether `accelerator` can be used as the Memory Banks shortcut.
MemoryBanksShortcutStatus ValidateShortcut(const ui::Accelerator& accelerator) {
  if (!IsSupportedKey(accelerator.key_code())) {
    return MemoryBanksShortcutStatus::kInvalid;
  }

  // At least one of Ctrl, Alt or Command is needed. Shift by itself is not
  // enough, since it is used to type characters.
  if (!accelerator.IsCtrlDown() && !accelerator.IsAltDown() &&
      !accelerator.IsCmdDown()) {
    return MemoryBanksShortcutStatus::kInvalid;
  }

  if (HasDisallowedModifiers(accelerator)) {
    return MemoryBanksShortcutStatus::kDisallowedModifiers;
  }

  // Checks the shortcuts Chrome actually uses on this platform: the
  // accelerator table on Windows and Linux, plus the default system shortcuts
  // on ChromeOS, and the main menu and other shortcuts on macOS. This is the
  // same check that keeps extension shortcuts from overriding Chrome's.
  if (IsChromeAccelerator(accelerator)) {
    return MemoryBanksShortcutStatus::kConflictsWithBrowserShortcut;
  }

  return MemoryBanksShortcutStatus::kValid;
}

}  // namespace

ui::Accelerator GetBrowseMemoryBanksShortcutFromPrefs(
    const PrefService& prefs) {
  // The pref uses the `ui::Command` accelerator string format, for example
  // "Ctrl+Shift+K". An empty or malformed value parses to an empty
  // accelerator, which fails validation below.
  const ui::Accelerator accelerator = ui::Command::StringToAccelerator(
      prefs.GetString(prefs::kContextHubBrowseMemoryBanksShortcut));
  if (ValidateShortcut(accelerator) != MemoryBanksShortcutStatus::kValid) {
    return ui::Accelerator();
  }
  return accelerator;
}

MemoryBanksShortcutStatus SetBrowseMemoryBanksShortcutInPrefs(
    PrefService& prefs,
    std::string_view shortcut) {
  if (shortcut.empty()) {
    prefs.ClearPref(prefs::kContextHubBrowseMemoryBanksShortcut);
    return MemoryBanksShortcutStatus::kValid;
  }

  const ui::Accelerator accelerator =
      ui::Command::StringToAccelerator(shortcut);
  const MemoryBanksShortcutStatus status = ValidateShortcut(accelerator);
  if (status != MemoryBanksShortcutStatus::kValid) {
    return status;
  }

  // Store the canonical form so that equivalent inputs are saved identically.
  prefs.SetString(prefs::kContextHubBrowseMemoryBanksShortcut,
                  ui::Command::AcceleratorToString(accelerator));
  return MemoryBanksShortcutStatus::kValid;
}

ui::Accelerator GetSuggestedBrowseMemoryBanksShortcut() {
  // Candidates in order of preference. M stands for "Memory".
  // K is a fallback that no built-in shortcut uses on any platform.
  static constexpr ui::Accelerator kCandidates[] = {
#if BUILDFLAG(IS_MAC)
      // Control+Command+M.
      ui::Accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN | ui::EF_COMMAND_DOWN),
      // Fallback: Command+Shift+K.
      ui::Accelerator(ui::VKEY_K, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN),
#elif BUILDFLAG(IS_CHROMEOS)
      // Alt+M.
      // (Alt+Shift+M is avoided because it opens the Files app.)
      ui::Accelerator(ui::VKEY_M, ui::EF_ALT_DOWN),
      // Fallback: Ctrl+Shift+K.
      ui::Accelerator(ui::VKEY_K, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
#else
      // Alt+Shift+M.
      ui::Accelerator(ui::VKEY_M, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN),
      // Fallback: Ctrl+Shift+K.
      ui::Accelerator(ui::VKEY_K, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN),
#endif
  };

  for (const ui::Accelerator& candidate : kCandidates) {
    if (ValidateShortcut(candidate) == MemoryBanksShortcutStatus::kValid) {
      return candidate;
    }
  }
  return ui::Accelerator();
}

}  // namespace context_hub
