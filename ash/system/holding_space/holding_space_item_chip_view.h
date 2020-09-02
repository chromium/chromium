// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
class ToggleImageButton;
}  // namespace views

namespace ash {

class HoldingSpaceItem;

namespace tray {
class RoundedImageView;
}  // namespace tray

// A button with an image derived from a file's thumbnail and file's name as the
// label.
class ASH_EXPORT HoldingSpaceItemChipView : public views::InkDropHostView,
                                            public views::ButtonListener {
 public:
  explicit HoldingSpaceItemChipView(const HoldingSpaceItem* item);
  HoldingSpaceItemChipView(const HoldingSpaceItemChipView&) = delete;
  HoldingSpaceItemChipView& operator=(const HoldingSpaceItemChipView&) = delete;
  ~HoldingSpaceItemChipView() override;

  // views::InkDropHostView:
  const char* GetClassName() const override;
  SkColor GetInkDropBaseColor() const override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  void AddPinButton();
  void Update();

  const HoldingSpaceItem* const item_;
  tray::RoundedImageView* image_ = nullptr;
  views::Label* label_ = nullptr;
  views::ToggleImageButton* pin_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIP_VIEW_H_
