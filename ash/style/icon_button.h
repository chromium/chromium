// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_ICON_BUTTON_H_
#define ASH_STYLE_ICON_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// A circular ImageButton that can have small/medium/large different sizes. Each
// of them has the floating version, which do not have the background.
class IconButton : public views::ImageButton {
 public:
  METADATA_HEADER(IconButton);

  enum class Type {
    kSmall,
    kMedium,
    kLarge,
    kSmallFloating,
    kMediumFloating,
    kLargeFloating
  };

  IconButton(PressedCallback callback,
             Type type,
             const gfx::VectorIcon& icon,
             int accessible_name_id);
  IconButton(const IconButton&) = delete;
  IconButton& operator=(const IconButton&) = delete;
  ~IconButton() override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const Type type_;
  const gfx::VectorIcon& icon_;
};

}  // namespace ash

#endif  // ASH_STYLE_ICON_BUTTON_H_
