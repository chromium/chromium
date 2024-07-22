// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/ime_mojom_traits.h"

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace mojo {
using KeyEventUniquePtr = std::unique_ptr<ui::KeyEvent>;

bool StructTraits<arc::mojom::KeyEventDataDataView, KeyEventUniquePtr>::Read(
    arc::mojom::KeyEventDataDataView data,
    KeyEventUniquePtr* out) {
  const ui::EventType type =
      data.pressed() ? ui::EventType::kKeyPressed : ui::EventType::kKeyReleased;
  ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(
      static_cast<ui::KeyboardCode>(data.key_code()));
  if (data.scan_code() != 0)
    dom_code = ui::KeycodeConverter::EvdevCodeToDomCode(data.scan_code());

  int flags = 0;
  if (data.is_shift_down())
    flags |= ui::EF_SHIFT_DOWN;
  if (data.is_control_down())
    flags |= ui::EF_CONTROL_DOWN;
  if (data.is_alt_down())
    flags |= ui::EF_ALT_DOWN;
  if (data.is_capslock_on())
    flags |= ui::EF_CAPS_LOCK_ON;
  if (data.is_alt_gr_down())
    flags |= ui::EF_ALTGR_DOWN;
  if (data.is_repeat())
    flags |= ui::EF_IS_REPEAT;

  ui::KeyboardCode key_code;
  ui::DomKey dom_key;
  if (!ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()->Lookup(
          dom_code, flags, &dom_key, &key_code)) {
    return false;
  }

  *out = std::make_unique<ui::KeyEvent>(type, key_code, dom_code, flags,
                                        dom_key, base::TimeTicks::Now());
  return true;
}

}  // namespace mojo
