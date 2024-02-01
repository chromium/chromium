// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_TEST_CHILD_MODAL_PARENT_H_
#define ASH_WM_TEST_TEST_CHILD_MODAL_PARENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
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
                             public views::WidgetObserver {
 public:
  // Create and show a top-level window that hosts a modal parent. Returns the
  // widget delegate, which is owned by the widget and deleted on window close.
  static TestChildModalParent* Show(aura::Window* context);

  explicit TestChildModalParent(aura::Window* context);

  TestChildModalParent(const TestChildModalParent&) = delete;
  TestChildModalParent& operator=(const TestChildModalParent&) = delete;

  ~TestChildModalParent() override;

  // Returns the modal parent window hosted within the top-level window.
  aura::Window* GetModalParent();

  // Create, show, and returns a child-modal window.
  aura::Window* ShowModalChild();

 private:
  // Overridden from views::View:
  void Layout(PassKey) override;
  void AddedToWidget() override;

  // Overridden from WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void ButtonPressed();

  // The widget for the modal parent, a child of TestChildModalParent's Widget.
  std::unique_ptr<views::Widget> modal_parent_;

  // The button to toggle showing and hiding the child window. The child window
  // does not block input to this button.
  raw_ptr<views::Button> button_;

  // The text field to indicate the keyboard focus.
  raw_ptr<views::Textfield> textfield_;

  // The host for the modal parent.
  raw_ptr<views::NativeViewHost> host_;

  // The modal child widget.
  raw_ptr<views::Widget> modal_child_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_TEST_TEST_CHILD_MODAL_PARENT_H_
