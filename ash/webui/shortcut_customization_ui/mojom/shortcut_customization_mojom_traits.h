// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_MOJOM_SHORTCUT_CUSTOMIZATION_MOJOM_TRAITS_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_MOJOM_SHORTCUT_CUSTOMIZATION_MOJOM_TRAITS_H_

#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-forward.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace mojo {

template <>
struct StructTraits<
    ash::shortcut_customization::mojom::SimpleAcceleratorDataView,
    ui::Accelerator> {
  static ui::KeyboardCode key_code(const ui::Accelerator& accelerator);
  static int modifiers(const ui::Accelerator& accelerator);
  static ui::Accelerator::KeyState key_state(
      const ui::Accelerator& accelerator);

  static bool Read(
      ash::shortcut_customization::mojom::SimpleAcceleratorDataView data,
      ui::Accelerator* out);
};

}  // namespace mojo

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_MOJOM_SHORTCUT_CUSTOMIZATION_MOJOM_TRAITS_H_
