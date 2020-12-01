// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_

#include "ui/views/controls/button/image_button.h"

namespace ash {

// Collapse button shown in TopShortcutsView with TopShortcutButtons.
// UnifiedSystemTrayBubble will support collapsed state where the height of the
// bubble is smaller, and some rows and labels will be omitted.
// By pressing the button, the state of the bubble will be toggled.
class CollapseButton : public views::ImageButton {
 public:
  explicit CollapseButton(PressedCallback callback);
  ~CollapseButton() override;

  // Change the expanded state. The icon will change.
  void SetExpandedAmount(double expanded_amount);

  // views::ImageButton:
  gfx::Size CalculatePreferredSize() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

 private:
  double expanded_amount_ = 1.0;

  DISALLOW_COPY_AND_ASSIGN(CollapseButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_COLLAPSE_BUTTON_H_
