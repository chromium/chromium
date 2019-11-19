// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_DELEGATE_H_
#define ASH_SHELF_SHELF_BUTTON_DELEGATE_H_

#include "ui/events/event.h"

namespace ui {
class Event;
}

namespace views {
class Button;
class InkDrop;
}  // namespace views

namespace ash {
class ShelfButton;

// ShelfButtonDelegate is an interface to allow ShelfButtons to notify their
// host view when they are pressed.
// TODO(mohsen): A better approach would be to return a value indicating the
// type of action performed such that the button can animate the ink drop.
// Currently, it is not possible because showing menu is synchronous and blocks
// the call. Fix this after menu is converted to asynchronous.  Long-term, the
// return value can be merged into ButtonListener.
class ShelfButtonDelegate {
 public:
  ShelfButtonDelegate() {}
  ~ShelfButtonDelegate() = default;

  // Used to let the host view redirect focus.
  virtual void OnShelfButtonAboutToRequestFocusFromTabTraversal(
      ShelfButton* button,
      bool reverse) = 0;

  // Notify the host view that the button was pressed. |ink_drop| is used to do
  // appropriate ink drop animation based on the action performed.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event,
                             views::InkDrop* ink_drop) = 0;

  // Called when the shelf button handles the accessible action with type of
  // kScrollToMakeVisible. |button| is the view receiving the accessibility
  // focus.
  virtual void HandleAccessibleActionScrollToMakeVisible(ShelfButton* button) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfButtonDelegate);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_DELEGATE_H_
