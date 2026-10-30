// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANKS_SHORTCUT_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANKS_SHORTCUT_H_

#include <string_view>

class PrefService;

namespace ui {
class Accelerator;
}  // namespace ui

namespace context_hub {

// Result of validating a user-configured Memory Banks keyboard shortcut.
enum class MemoryBanksShortcutStatus {
  // The shortcut can be used.
  kValid,
  // The shortcut could not be parsed, does not use a supported key (A-Z, 0-9),
  // or does not include Ctrl, Alt or Command.
  kInvalid,
  // The shortcut uses a modifier combination reserved by the operating system
  // (for example, Ctrl+Alt on Windows and Linux).
  kDisallowedModifiers,
  // The shortcut is already used by Chrome.
  kConflictsWithBrowserShortcut,
};

// Returns the shortcut stored in `prefs`, or an empty accelerator if no
// shortcut is set or the stored shortcut is no longer valid.
ui::Accelerator GetBrowseMemoryBanksShortcutFromPrefs(const PrefService& prefs);

// Saves `shortcut` (in `ui::Command` format, for example "Ctrl+Shift+K") to
// `prefs` if it is valid. An empty `shortcut` clears the pref. Returns `kValid`
// on success; otherwise the pref is left unchanged.
MemoryBanksShortcutStatus SetBrowseMemoryBanksShortcutInPrefs(
    PrefService& prefs,
    std::string_view shortcut);

// Returns a platform-appropriate shortcut to suggest to the user (for example,
// Alt+Shift+M on Windows and Linux, or Control+Command+M on macOS). The
// suggestion is never applied automatically. Returns an empty accelerator if no
// candidate is valid.
ui::Accelerator GetSuggestedBrowseMemoryBanksShortcut();

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANKS_SHORTCUT_H_
