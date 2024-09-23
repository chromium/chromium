// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_shortcuts.h"

#include "ash/picker/picker_search_result.h"
#include "ash/shell.h"
#include "base/notreached.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {

PickerCapsLockResult::Shortcut GetPickerShortcutForCapsLock() {
  // The Shell may not exist in some tests. In this case, return the shortcut
  // for the default keyboard.
  if (!Shell::HasInstance()) {
    return PickerCapsLockResult::Shortcut::kAltSearch;
  }

  if (Shell::Get()->keyboard_capability()->HasFunctionKeyOnAnyKeyboard()) {
    return PickerCapsLockResult::Shortcut::kFnRightAlt;
  }

  switch (Shell::Get()->keyboard_capability()->GetMetaKeyToDisplay()) {
    case ui::mojom::MetaKey::kSearch:
      return PickerCapsLockResult::Shortcut::kAltSearch;
    case ui::mojom::MetaKey::kLauncher:
    case ui::mojom::MetaKey::kLauncherRefresh:
      return PickerCapsLockResult::Shortcut::kAltLauncher;
    case ui::mojom::MetaKey::kExternalMeta:
    case ui::mojom::MetaKey::kCommand:
      NOTREACHED_NORETURN();
  }
}

}  // namespace ash
