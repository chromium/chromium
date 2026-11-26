// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/shortcut.h"

#include "build/build_config.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "ui/base/accelerators/command.h"

namespace ttc {

namespace {

bool HasDisallowedModifiers(const ui::Accelerator& accelerator) {
#if BUILDFLAG(IS_MAC)
  // Option (and Shift+Option) with neither Command nor Control is used to type
  // accented and special characters.
  return accelerator.IsAltDown() && !accelerator.IsCmdDown() &&
         !accelerator.IsCtrlDown();
#else
  // Ctrl+Alt is the same as AltGr, which international keyboard layouts need
  // to type characters. The Windows key, the ChromeOS Search key and the
  // Android Meta key are reserved by the operating system.
  return (accelerator.IsCtrlDown() && accelerator.IsAltDown()) ||
         accelerator.IsCmdDown();
#endif
}

}  // namespace

ShortcutStatus ValidateShortcut(const ui::Accelerator& accelerator) {
  // At least one of Ctrl, Alt or Command is needed. Shift by itself is not
  // enough, since it is used to type characters.
  if (!accelerator.IsCtrlDown() && !accelerator.IsAltDown() &&
      !accelerator.IsCmdDown()) {
    return ShortcutStatus::kInvalid;
  }

  if (HasDisallowedModifiers(accelerator)) {
    return ShortcutStatus::kInvalid;
  }

  // The prefs hold ui::Command strings, which every platform can read, so the
  // shortcut must be one a string can name exactly. This rules out keys a
  // string can't name, and other modifiers, such as AltGr.
  const ui::Accelerator canonical = ui::Command::StringToAccelerator(
      ui::Command::AcceleratorToString(accelerator));
  if (canonical != accelerator) {
    return ShortcutStatus::kInvalid;
  }

  // The same check that keeps extension shortcuts from overriding Chrome's.
  // It gets `canonical`, which has only modifier flags: `accelerator` can also
  // have EF_IS_REPEAT or EF_IS_SYNTHESIZED, which operator== ignores, but some
  // platforms' IsChromeAccelerator() doesn't.
  if (IsChromeAccelerator(canonical)) {
    return ShortcutStatus::kConflictsWithBrowserShortcut;
  }

  return ShortcutStatus::kValid;
}

ui::Accelerator ParseShortcut(std::string_view pref_value) {
  if (pref_value.empty()) {
    return ui::Accelerator();
  }
  ui::Accelerator accelerator = ui::Command::StringToAccelerator(pref_value);
  if (accelerator.IsEmpty() ||
      ValidateShortcut(accelerator) != ShortcutStatus::kValid) {
    return ui::Accelerator();
  }
  return accelerator;
}

}  // namespace ttc
