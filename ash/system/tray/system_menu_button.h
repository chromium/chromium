// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
#define ASH_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A 48x48 image button with a material design ripple effect, which can be
// used across Ash material design native UI menus.
class SystemMenuButton : public views::ImageButton {
  METADATA_HEADER(SystemMenuButton, views::ImageButton)

 public:
  // Constructs the button with |callback| and a centered icon corresponding to
  // |normal_icon| when button is enabled and |disabled_icon| when it is
  // disabled. |accessible_name_id| corresponds to the string in
  // ui::ResourceBundle to use for the button's accessible and tooltip text.
  SystemMenuButton(PressedCallback callback,
                   const gfx::ImageSkia& normal_icon,
                   const gfx::ImageSkia& disabled_icon,
                   int accessible_name_id);

  // Similar to the above constructor. Just gets a single vector icon and
  // creates the normal and disabled icons based on that using default menu icon
  // colors.
  SystemMenuButton(PressedCallback callback,
                   const gfx::VectorIcon& icon,
                   int accessible_name_id);

  SystemMenuButton(const SystemMenuButton&) = delete;
  SystemMenuButton& operator=(const SystemMenuButton&) = delete;

  ~SystemMenuButton() override;

  // Sets the normal and disabled icons based on that using default menu icon
  // colors.
  void SetVectorIcon(const gfx::VectorIcon& icon);

 private:
  // Returns the size that the ink drop should be constructed with.
  gfx::Size GetInkDropSize() const;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
