// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SHORTCUT_H_
#define CHROME_BROWSER_TTC_CORE_SHORTCUT_H_

#include <string_view>

#include "ui/base/accelerators/accelerator.h"

namespace ttc {

// Whether a key combination can be a TTC keyboard shortcut.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ttc.settings
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: TtcShortcutStatus
enum class ShortcutStatus {
  kValid = 0,
  // Not a combination TTC shortcuts can use, e.g. it has no Ctrl or Alt, or
  // its key has no name in the TTC shortcut prefs.
  kInvalid = 1,
  // One of Chrome's own keyboard shortcuts.
  kConflictsWithBrowserShortcut = 2,
};

// Returns whether `accelerator` can be a TTC keyboard shortcut.
ShortcutStatus ValidateShortcut(const ui::Accelerator& accelerator);

// Returns the accelerator a TTC shortcut pref value names, such as
// "Alt+Shift+K", or an empty accelerator if the value is empty or isn't a
// valid shortcut.
ui::Accelerator ParseShortcut(std::string_view pref_value);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SHORTCUT_H_
