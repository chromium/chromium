// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_CONTROL_BUTTON_H_
#define ASH_SHELF_SHELF_CONTROL_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class ShelfButtonDelegate;

// Base class for controls shown on the shelf that are not app shortcuts, such
// as the app list, back, and overflow buttons.
class ASH_EXPORT ShelfControlButton : public ShelfButton {
  METADATA_HEADER(ShelfControlButton, ShelfButton)

 public:
  ShelfControlButton(Shelf* shelf, ShelfButtonDelegate* shelf_button_delegate_);

  ShelfControlButton(const ShelfControlButton&) = delete;
  ShelfControlButton& operator=(const ShelfControlButton&) = delete;

  ~ShelfControlButton() override;

  // Get the center point of the button used to draw its background and ink
  // drops.
  gfx::PointF GetCenterPoint() const;

  const gfx::Rect& ideal_bounds() const { return ideal_bounds_; }

  void set_ideal_bounds(const gfx::Rect& bounds) { ideal_bounds_ = bounds; }

  // ShelfButton:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  gfx::Rect ideal_bounds_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_CONTROL_BUTTON_H_
