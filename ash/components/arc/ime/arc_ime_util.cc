// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"

namespace arc {

bool IsControlChar(const ui::KeyEvent* event) {
  // 0x00-0x1f (C0 controls), 0x7f (DEL), and 0x80-0x9f (C1 controls) are
  // considered as a control character. See:
  // https://en.wikipedia.org/wiki/Unicode_control_characters They are control
  // characters and not treated as a text insertion.
  const char16_t ch = event->GetCharacter();
  const bool is_control_char =
      (0x00 <= ch && ch <= 0x1f) || (0x7f <= ch && ch <= 0x9f);
  return is_control_char;
}

bool HasModifier(const ui::KeyEvent* event) {
  constexpr int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
  return (event->flags() & kModifierMask) != 0;
}

}  // namespace arc
