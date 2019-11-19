// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_CLOSE_DESK_BUTTON_H_
#define ASH_WM_DESKS_CLOSE_DESK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {

// A button view that shows up on hovering over the associated desk mini_view,
// which let users remove the mini_view and its corresponding desk.
class ASH_EXPORT CloseDeskButton : public views::ImageButton,
                                   public views::ViewTargeterDelegate {
 public:
  explicit CloseDeskButton(views::ButtonListener* listener);
  ~CloseDeskButton() override;

  // The size of the close button.
  static constexpr int kCloseButtonSize = 16;

  // views::ImageButton:
  const char* GetClassName() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  bool DoesIntersectScreenRect(const gfx::Rect& screen_rect) const;

 private:
  float highlight_opacity_ = 0.f;
  SkColor inkdrop_base_color_ = gfx::kPlaceholderColor;

  DISALLOW_COPY_AND_ASSIGN(CloseDeskButton);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_CLOSE_DESK_BUTTON_H_
