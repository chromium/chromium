// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_CHIP_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_CHIP_VIEW_H_

#include "base/component_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) ChipView : public views::Button {
  METADATA_HEADER(ChipView, views::Button)

 public:
  enum Type { kDefault, kLarge };

  static constexpr int kIconSizeDip = 16;

  explicit ChipView(Type type);
  ~ChipView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildVisibilityChanged(views::View* child) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // Make `icon_view_` visible to secure layout space for it. Use this method to
  // avoid layout shift after downloading an icon image from the Web.
  void MakeIconVisible();
  void SetIcon(const gfx::ImageSkia& icon);
  gfx::ImageSkia GetIcon() const;

  void SetText(const std::u16string& text);
  const std::u16string& GetText() const;

 private:
  const Type type_;
  raw_ptr<views::BoxLayout> layout_manager_;
  raw_ptr<views::ImageView> icon_view_;
  raw_ptr<views::Label> text_view_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_CHIP_VIEW_H_
