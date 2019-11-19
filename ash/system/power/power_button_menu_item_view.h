// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_BUTTON_MENU_ITEM_VIEW_H_
#define ASH_SYSTEM_POWER_BUTTON_MENU_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// PowerButtonMenuItemView represents an item of the power button menu. It
// includes an icon and title.
class ASH_EXPORT PowerButtonMenuItemView : public views::ImageButton {
 public:
  // Height of the menu item in pixels.
  static constexpr int kMenuItemHeight = 84;
  // Width of the menu item in pixels.
  static constexpr int kMenuItemWidth = 84;

  // Thickness of the menu item's border in pixels.
  static constexpr int kItemBorderThickness = 2;

  PowerButtonMenuItemView(views::ButtonListener* listener,
                          const gfx::VectorIcon& icon,
                          const base::string16& title_text);
  ~PowerButtonMenuItemView() override;

  // views::View:
  const char* GetClassName() const override;

 private:
  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnFocus() override;
  void OnBlur() override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Owned by views hierarchy.
  views::ImageView* icon_view_ = nullptr;
  views::Label* title_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PowerButtonMenuItemView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_BUTTON_MENU_ITEM_VIEW_H_
