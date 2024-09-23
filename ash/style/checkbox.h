// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_CHECKBOX_H_
#define ASH_STYLE_CHECKBOX_H_

#include "ash/ash_export.h"
#include "ash/style/option_button_base.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A rectangular label button with the icon on its left side. It's usually used
// in the group of checkboxes. Please refer `CheckboxGroup` for more details.
class ASH_EXPORT Checkbox : public OptionButtonBase {
  METADATA_HEADER(Checkbox, OptionButtonBase)

 public:
  explicit Checkbox(int button_width,
                    PressedCallback callback,
                    const std::u16string& label = std::u16string(),
                    const gfx::Insets& insets = kDefaultPadding,
                    int image_label_spacing = kImageLabelSpacingDP);
  Checkbox(const Checkbox&) = delete;
  Checkbox& operator=(const Checkbox&) = delete;
  ~Checkbox() override;

  // OptionButtonBase:
  gfx::ImageSkia GetImage(ButtonState for_state) const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool IsIconOnTheLeftSide() override;
};

}  // namespace ash

#endif  // ASH_STYLE_CHECKBOX_H_