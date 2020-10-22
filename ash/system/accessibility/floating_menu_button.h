// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace gfx {
struct VectorIcon;
class Size;
}  // namespace gfx

namespace ash {

// Button view that is used in floating menu.

class FloatingMenuButton : public views::ImageButton {
 public:
  FloatingMenuButton(views::ButtonListener* listener,
                     const gfx::VectorIcon& icon,
                     int accessible_name_id,
                     bool flip_for_rtl);

  FloatingMenuButton(views::ButtonListener* listener,
                     const gfx::VectorIcon& icon,
                     int accessible_name_id,
                     bool flip_for_rtl,
                     int size,
                     bool draw_highlight,
                     bool is_a11y_togglable);

  ~FloatingMenuButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Change the toggle state.
  void SetToggled(bool toggled);

  bool IsToggled() { return toggled_; }

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  // Used in tests.
  void SetId(int id);

 private:
  void UpdateImage();

  const gfx::VectorIcon* icon_;
  // True if the button is currently toggled.
  bool toggled_ = false;
  int size_;
  const bool draw_highlight_;
  // Whether this button will be described as togglable to screen reading tools.
  const bool is_a11y_togglable_;

  DISALLOW_COPY_AND_ASSIGN(FloatingMenuButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_
