// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_BACK_BUTTON_H_
#define ASH_SHELF_BACK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_control_button.h"
#include "base/macros.h"

namespace ash {

class ShelfButtonDelegate;

// The back button shown on the shelf when tablet mode is enabled. Its opacity
// and visiblity are handled by its parent, ShelfView, to ensure the fade
// in/out of the icon matches the movement of ShelfView's items.
class ASH_EXPORT BackButton : public ShelfControlButton,
                              public ShelfButtonDelegate {
 public:
  static const char kViewClassName[];

  explicit BackButton(Shelf* shelf);
  ~BackButton() override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  base::string16 GetTooltipText(const gfx::Point& p) const override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;

  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackButton);
};

}  // namespace ash

#endif  // ASH_SHELF_BACK_BUTTON_H_
