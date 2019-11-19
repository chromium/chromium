// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
#define ASH_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A 48x48 image button with a material design ripple effect, which can be
// used across Ash material design native UI menus.
class SystemMenuButton : public views::ImageButton {
 public:
  // Constructs the button with |listener| and a centered icon corresponding to
  // |normal_icon| when button is enabled and |disabled_icon| when it is
  // disabled. |accessible_name_id| corresponds to the string in
  // ui::ResourceBundle to use for the button's accessible and tooltip text.
  SystemMenuButton(views::ButtonListener* listener,
                   const gfx::ImageSkia& normal_icon,
                   const gfx::ImageSkia& disabled_icon,
                   int accessible_name_id);

  // Similar to the above constructor. Just gets a single vector icon and
  // creates the normal and disabled icons based on that using default menu icon
  // colors.
  SystemMenuButton(views::ButtonListener* listener,
                   const gfx::VectorIcon& icon,
                   int accessible_name_id);
  ~SystemMenuButton() override;

  // Sets the normal and disabled icons based on that using default menu icon
  // colors.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Explicity sets the ink drop color. Otherwise the default value will be used
  // by TrayPopupUtils::CreateInkDropRipple() and
  // TrayPopupUtils::CreateInkDropHighlight().
  void SetInkDropColor(SkColor color);

  // views::ImageButton:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  const char* GetClassName() const override;

 private:
  // Returns the size that the ink drop should be constructed with.
  gfx::Size GetInkDropSize() const;

  // The color to use when creating the ink drop. If null the default color is
  // used as defined by TrayPopupUtils::CreateInkDropRipple() and
  // TrayPopupUtils::CreateInkDropHighlight().
  base::Optional<SkColor> ink_drop_color_;

  DISALLOW_COPY_AND_ASSIGN(SystemMenuButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
