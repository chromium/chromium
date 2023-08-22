// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization_mojom_traits.h"

#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace mojo {

namespace {

// These are valid modifiers for an accelerator.
const int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                          ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                          ui::EF_FUNCTION_DOWN | ui::EF_ALTGR_DOWN |
                          ui::EF_IS_SYNTHESIZED | ui::EF_IS_REPEAT;

}  // namespace

ui::KeyboardCode
StructTraits<ash::shortcut_customization::mojom::SimpleAcceleratorDataView,
             ui::Accelerator>::key_code(const ui::Accelerator& accelerator) {
  return accelerator.key_code();
}

int StructTraits<ash::shortcut_customization::mojom::SimpleAcceleratorDataView,
                 ui::Accelerator>::modifiers(const ui::Accelerator&
                                                 accelerator) {
  // Bitmask the modifiers so that we ensure that only relevant accelerator
  // modifiers are present.
  return accelerator.modifiers() & kModifierMask;
}

ui::Accelerator::KeyState
StructTraits<ash::shortcut_customization::mojom::SimpleAcceleratorDataView,
             ui::Accelerator>::key_state(const ui::Accelerator& accelerator) {
  return accelerator.key_state();
}

bool StructTraits<ash::shortcut_customization::mojom::SimpleAcceleratorDataView,
                  ui::Accelerator>::
    Read(ash::shortcut_customization::mojom::SimpleAcceleratorDataView data,
         ui::Accelerator* out) {
  ui::KeyboardCode keycode;
  if (!data.ReadKeyCode(&keycode)) {
    return false;
  }

  ui::Accelerator::KeyState key_state;
  if (!data.ReadKeyState(&key_state)) {
    return false;
  }

  *out = ui::Accelerator(keycode, data.modifiers() & kModifierMask, key_state);
  return true;
}

}  // namespace mojo
