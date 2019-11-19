// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_TOP_SHORTCUT_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_TOP_SHORTCUT_BUTTON_H_

#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A button used in top shortcuts. Top shortcuts are small circular buttons
// shown on top of the UnifiedSystemTrayView that allows quick access to
// frequently used features e.g. lock screen, settings, and shutdown.
class TopShortcutButton : public views::ImageButton {
 public:
  explicit TopShortcutButton(const gfx::VectorIcon& icon,
                             int accessible_name_id);
  TopShortcutButton(views::ButtonListener* listener,
                    const gfx::VectorIcon& icon,
                    int accessible_name_id);
  TopShortcutButton(views::ButtonListener* listener, int accessible_name_id);
  ~TopShortcutButton() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TopShortcutButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_TOP_SHORTCUT_BUTTON_H_
