// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/checkbox.h"

#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {}  // namespace

Checkbox::Checkbox(int button_width,
                   PressedCallback callback,
                   const std::u16string& label,
                   const gfx::Insets& insets,
                   int image_label_spacing)
    : OptionButtonBase(button_width,
                       std::move(callback),
                       label,
                       insets,
                       image_label_spacing) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kCheckBox);
}

Checkbox::~Checkbox() = default;

gfx::ImageSkia Checkbox::GetImage(ButtonState for_state) const {
  return gfx::CreateVectorIcon(GetVectorIcon(), kIconSize, GetIconImageColor());
}

const gfx::VectorIcon& Checkbox::GetVectorIcon() const {
  return selected() ? views::kCheckboxActiveIcon : views::kCheckboxNormalIcon;
}

bool Checkbox::IsIconOnTheLeftSide() {
  return true;
}

BEGIN_METADATA(Checkbox)
END_METADATA

}  // namespace ash
