// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_ITEM_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(PowerButtonMenuItemView, views::ImageButton)

 public:
  // Height of the menu item in pixels.
  static constexpr int kMenuItemHeight = 84;
  // Width of the menu item in pixels.
  static constexpr int kMenuItemWidth = 84;

  // Thickness of the menu item's border in pixels.
  static constexpr int kItemBorderThickness = 2;

  PowerButtonMenuItemView(PressedCallback callback,
                          const gfx::VectorIcon& icon,
                          const std::u16string& title_text);
  PowerButtonMenuItemView(const PowerButtonMenuItemView&) = delete;
  PowerButtonMenuItemView& operator=(const PowerButtonMenuItemView&) = delete;
  ~PowerButtonMenuItemView() override;

 private:
  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnFocus() override;
  void OnBlur() override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Owned by views hierarchy.
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;

  const raw_ref<const gfx::VectorIcon> icon_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_ITEM_VIEW_H_
