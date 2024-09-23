// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_H_
#define ASH_SHELF_SHELF_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class Shelf;
class ShelfButtonDelegate;

// Button used for items on the shelf.
class ASH_EXPORT ShelfButton : public views::Button {
  METADATA_HEADER(ShelfButton, views::Button)

 public:
  ShelfButton(Shelf* shelf, ShelfButtonDelegate* shelf_button_delegate);

  ShelfButton(const ShelfButton&) = delete;
  ShelfButton& operator=(const ShelfButton&) = delete;

  ~ShelfButton() override;

  // views::Button:
  void OnThemeChanged() override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void NotifyClick(const ui::Event& event) override;

  Shelf* shelf() { return shelf_; }
  const Shelf* shelf() const { return shelf_; }

 protected:
  ShelfButtonDelegate* shelf_button_delegate() {
    return shelf_button_delegate_;
  }

 private:
  // The shelf instance that this button belongs to. Unowned.
  const raw_ptr<Shelf> shelf_;

  // A class to which this button delegates handling some of its events.
  const raw_ptr<ShelfButtonDelegate> shelf_button_delegate_;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_H_
