// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_H_
#define ASH_SHELF_SHELF_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class Shelf;
class ShelfButtonDelegate;

// Button used for items on the shelf.
class ASH_EXPORT ShelfButton : public views::Button {
 public:
  ShelfButton(Shelf* shelf, ShelfButtonDelegate* shelf_button_delegate);
  ~ShelfButton() override;

  // views::Button:
  const char* GetClassName() const override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void NotifyClick(const ui::Event& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;

  Shelf* shelf() { return shelf_; }

 protected:
  ShelfButtonDelegate* shelf_button_delegate() {
    return shelf_button_delegate_;
  }

 private:
  // The shelf instance that this button belongs to. Unowned.
  Shelf* const shelf_;

  // A class to which this button delegates handling some of its events.
  ShelfButtonDelegate* const shelf_button_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButton);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_H_
