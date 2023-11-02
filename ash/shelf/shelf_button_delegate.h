// Copyright 2019 The Chromium Authors
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
// the call. Fix this after menu is converted to asynchronous.
class ShelfButtonDelegate {
 public:
  class ScopedActiveInkDropCount {
   public:
    virtual ~ScopedActiveInkDropCount() = default;
  };

  ShelfButtonDelegate() {}

  ShelfButtonDelegate(const ShelfButtonDelegate&) = delete;
  ShelfButtonDelegate& operator=(const ShelfButtonDelegate&) = delete;

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

  // Returns a scoped count that indicates whether |button| has an active ink
  // drop. |button| calls this to get the scoped count when its ink drop is
  // activated. It holds on to the scoped count until the ink drop is no longer
  // active.
  virtual std::unique_ptr<ScopedActiveInkDropCount>
  CreateScopedActiveInkDropCount(const ShelfButton* button);

  // Notifies the host view that one button will be removed.
  virtual void OnButtonWillBeRemoved() {}

  // Notifies the host view that the app button `button` is activated.
  virtual void OnAppButtonActivated(const ShelfButton* button) {}
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_DELEGATE_H_
