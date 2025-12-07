// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/radio_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"

namespace ash {

namespace {}  // namespace

RadioButton::RadioButton(int button_width,
                         PressedCallback callback,
                         const std::u16string& label,
                         IconDirection icon_direction,
                         IconType icon_type,
                         const gfx::Insets& insets,
                         int image_label_spacing)
    : OptionButtonBase(button_width,
                       std::move(callback),
                       label,
                       insets,
                       image_label_spacing),
      icon_direction_(icon_direction),
      icon_type_(icon_type) {
  GetViewAccessibility().SetRole(ax::mojom::Role::kRadioButton);
}

RadioButton::~RadioButton() = default;

gfx::ImageSkia RadioButton::GetImage(ButtonState for_state) const {
  // For check icon type button, no image icon to show when it's unselected.
  if (!selected() && icon_type_ == IconType::kCheck)
    return gfx::ImageSkia();

  return gfx::CreateVectorIcon(GetVectorIcon(), kIconSize, GetIconImageColor());
}

const gfx::VectorIcon& RadioButton::GetVectorIcon() const {
  if (icon_type_ == IconType::kCircle) {
    return selected() ? views::kRadioButtonActiveIcon
                      : views::kRadioButtonNormalIcon;
  }
  return kHollowCheckCircleIcon;
}

bool RadioButton::IsIconOnTheLeftSide() {
  return icon_direction_ == IconDirection::kLeading;
}

BEGIN_METADATA(RadioButton)
END_METADATA

}  // namespace ash
