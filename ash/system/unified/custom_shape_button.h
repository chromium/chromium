// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_CUSTOM_SHAPE_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_CUSTOM_SHAPE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace ash {

// Abstract base class of buttons that have custom shape with Material Design
// ink drop.
class CustomShapeButton : public views::ImageButton {
 public:
  explicit CustomShapeButton(views::ButtonListener* listener);
  ~CustomShapeButton() override;

  // Return the custom shape for the button in SkPath.
  virtual SkPath CreateCustomShapePath(const gfx::Rect& bounds) const = 0;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  const char* GetClassName() const override;

 protected:
  void PaintCustomShapePath(gfx::Canvas* canvas);

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomShapeButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_CUSTOM_SHAPE_BUTTON_H_
