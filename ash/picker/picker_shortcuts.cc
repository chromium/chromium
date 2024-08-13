// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_shortcuts.h"

#include "ash/public/cpp/picker/picker_search_result.h"

namespace ash {

PickerSearchResult::CapsLockData::Shortcut GetPickerShortcutForCapsLock() {
  // TODO: b/350385392 - Get the correct shortcut for each keyboard capability.
  return PickerSearchResult::CapsLockData::Shortcut::kAltSearch;
}

}  // namespace ash
