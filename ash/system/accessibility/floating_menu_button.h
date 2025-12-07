// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_

#include "ash/system/tray/tray_constants.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/metadata/view_factory.h"

namespace gfx {
struct VectorIcon;
class Size;
}  // namespace gfx

namespace ash {

// Button view that is used in floating menu.

class FloatingMenuButton : public views::ImageButton {
  METADATA_HEADER(FloatingMenuButton, views::ImageButton)

 public:
  FloatingMenuButton();
  FloatingMenuButton(views::Button::PressedCallback callback,
                     const gfx::VectorIcon& icon,
                     int accessible_name_id,
                     bool flip_for_rtl);

  FloatingMenuButton(views::Button::PressedCallback callback,
                     const gfx::VectorIcon& icon,
                     int accessible_name_id,
                     bool flip_for_rtl,
                     int size,
                     bool draw_highlight,
                     bool is_a11y_togglable);
  FloatingMenuButton(const FloatingMenuButton&) = delete;
  FloatingMenuButton& operator=(const FloatingMenuButton&) = delete;

  ~FloatingMenuButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  bool GetA11yTogglable() const;
  void SetA11yTogglable(bool a11y_togglable);

  bool GetDrawHighlight() const;
  void SetDrawHighlight(bool draw_highlight);

  // Toggle state property.
  bool GetToggled() const;
  void SetToggled(bool toggled);

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void UpdateImage();
  void UpdateAccessibleProperties();

  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;
  // True if the button is currently toggled.
  bool toggled_ = false;
  int size_ = kTrayItemSize;
  bool draw_highlight_ = true;
  // Whether this button will be described as togglable to screen reading tools.
  bool is_a11y_togglable_ = true;
};

BEGIN_VIEW_BUILDER(/* no export */, FloatingMenuButton, views::ImageButton)
VIEW_BUILDER_PROPERTY(bool, A11yTogglable)
VIEW_BUILDER_PROPERTY(bool, DrawHighlight)
VIEW_BUILDER_PROPERTY(bool, Toggled)
VIEW_BUILDER_PROPERTY(const gfx::VectorIcon&,
                      VectorIcon,
                      const gfx::VectorIcon&)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::FloatingMenuButton)

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_
