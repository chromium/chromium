// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUTTON_H_
#define ASH_SHELF_OVERFLOW_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_control_button.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
}

namespace ash {
class ShelfView;

// Shelf overflow button.
class ASH_EXPORT OverflowButton : public ShelfControlButton {
 public:
  // |shelf_view| is the view containing this button.
  explicit OverflowButton(ShelfView* shelf_view);
  ~OverflowButton() override;

  // views::Button
  bool ShouldEnterPushedState(const ui::Event& event) override;
  void NotifyClick(const ui::Event& event) override;
  const char* GetClassName() const override;

 private:
  friend class OverflowButtonTestApi;

  const gfx::ImageSkia horizontal_dots_image_;
  views::ImageView* horizontal_dots_image_view_;
  // Owned by RootWindowController.
  ShelfView* shelf_view_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUTTON_H_
