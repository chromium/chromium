// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUTTON_H_
#define ASH_SHELF_OVERFLOW_BUTTON_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
}

namespace ash {

class Shelf;
class ShelfView;

// Shelf overflow button.
class ASH_EXPORT OverflowButton : public views::Button {
 public:
  // |shelf_view| is the view containing this button.
  OverflowButton(ShelfView* shelf_view, Shelf* shelf);
  ~OverflowButton() override;

 private:
  friend class OverflowButtonTestApi;

  // views::Button:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  void NotifyClick(const ui::Event& event) override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Helper function to paint the background of the button at |bounds|.
  void PaintBackground(gfx::Canvas* canvas, const gfx::Rect& bounds);

  // Calculates the bounds of the overflow button based on the shelf alignment
  // and overflow shelf visibility.
  gfx::Rect CalculateButtonBounds() const;

  // The icon in the new UI: horizontal dots.
  const gfx::ImageSkia horizontal_dots_image_;
  views::ImageView* horizontal_dots_image_view_;

  ShelfView* shelf_view_;
  Shelf* shelf_;

  // Color used to paint the background.
  SkColor background_color_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUTTON_H_
