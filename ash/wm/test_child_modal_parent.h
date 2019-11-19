// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_CHILD_MODAL_PARENT_H_
#define ASH_WM_TEST_CHILD_MODAL_PARENT_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class NativeViewHost;
class Textfield;
class View;
class Widget;
}  // namespace views

namespace ash {

// Test window that can act as a parent for modal child windows.
class TestChildModalParent : public views::WidgetDelegateView,
                             public views::ButtonListener,
                             public views::WidgetObserver {
 public:
  // Create and show a top-level window that hosts a modal parent. Returns the
  // widget delegate, which is owned by the widget and deleted on window close.
  static TestChildModalParent* Show(aura::Window* context);

  explicit TestChildModalParent(aura::Window* context);
  ~TestChildModalParent() override;

  // Returns the modal parent window hosted within the top-level window.
  aura::Window* GetModalParent() const;

  // Create, show, and returns a child-modal window.
  aura::Window* ShowModalChild();

 private:
  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // Overridden from views::View:
  void Layout() override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // Overridden from ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // The widget for the modal parent, a child of TestChildModalParent's Widget.
  std::unique_ptr<views::Widget> modal_parent_;

  // The button to toggle showing and hiding the child window. The child window
  // does not block input to this button.
  views::Button* button_;

  // The text field to indicate the keyboard focus.
  views::Textfield* textfield_;

  // The host for the modal parent.
  views::NativeViewHost* host_;

  // The modal child widget.
  views::Widget* modal_child_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestChildModalParent);
};

}  // namespace ash

#endif  // ASH_WM_TEST_CHILD_MODAL_PARENT_H_
