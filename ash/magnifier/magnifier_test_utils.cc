// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnifier_test_utils.h"

#include "ash/shell.h"
#include "ui/base/ime/input_method.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

aura::Window* GetViewRootWindow(views::View* view) {
  DCHECK(view);
  return view->GetWidget()->GetNativeWindow()->GetRootWindow();
}

gfx::Rect GetBoundsInRoot(const gfx::Rect& bounds_in_screen,
                          views::View* view) {
  gfx::Rect bounds = bounds_in_screen;
  ::wm::ConvertRectFromScreen(GetViewRootWindow(view), &bounds);
  return bounds;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TestFocusView:

// A view that contains two buttons positioned at constant bounds with ability
// to request focus on either one.
class TestFocusView : public views::WidgetDelegateView {
 public:
  TestFocusView()
      : button_1_(new views::LabelButton(nullptr, {})),
        button_2_(new views::LabelButton(nullptr, {})) {
    button_1_->SetFocusForPlatform();
    button_2_->SetFocusForPlatform();
    AddChildView(button_1_);
    AddChildView(button_2_);
  }

  ~TestFocusView() override = default;

  gfx::Size CalculatePreferredSize() const override {
    return MagnifierFocusTestHelper::kTestFocusViewSize;
  }

  void Layout() override {
    // Layout the first button at the top of the view.
    button_1_->SetBounds(0, 0,
                         MagnifierFocusTestHelper::kTestFocusViewSize.width(),
                         MagnifierFocusTestHelper::kButtonHeight);

    // And the second at the other end at the bottom of the view.
    button_2_->SetBounds(0,
                         MagnifierFocusTestHelper::kTestFocusViewSize.height() -
                             MagnifierFocusTestHelper::kButtonHeight,
                         MagnifierFocusTestHelper::kTestFocusViewSize.width(),
                         MagnifierFocusTestHelper::kButtonHeight);
  }

  views::LabelButton* button_1_;
  views::LabelButton* button_2_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFocusView);
};

////////////////////////////////////////////////////////////////////////////////
// TestTextInputView:

// A view that contains a single text field for testing text input events.
class TestTextInputView : public views::WidgetDelegateView {
 public:
  TestTextInputView() : text_field_(new views::Textfield) {
    text_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
    AddChildView(text_field_);
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  ~TestTextInputView() override = default;

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(50, 50);
  }

  void FocusOnTextInput() { text_field_->RequestFocus(); }

 private:
  views::Textfield* text_field_;  // owned by views hierarchy.

  DISALLOW_COPY_AND_ASSIGN(TestTextInputView);
};

////////////////////////////////////////////////////////////////////////////////
// MagnifierFocusTestHelper:

// static
constexpr int MagnifierFocusTestHelper::kButtonHeight;

// static
constexpr gfx::Size MagnifierFocusTestHelper::kTestFocusViewSize;

void MagnifierFocusTestHelper::CreateAndShowFocusTestView(
    const gfx::Point& location) {
  focus_test_view_ = new TestFocusView;
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      focus_test_view_, Shell::GetPrimaryRootWindow(),
      gfx::Rect(location, MagnifierFocusTestHelper::kTestFocusViewSize));
  widget->Show();
}

void MagnifierFocusTestHelper::FocusFirstButton() {
  DCHECK(focus_test_view_);
  focus_test_view_->button_1_->RequestFocus();
}

void MagnifierFocusTestHelper::FocusSecondButton() {
  DCHECK(focus_test_view_);
  focus_test_view_->button_2_->RequestFocus();
}

gfx::Rect MagnifierFocusTestHelper::GetFirstButtonBoundsInRoot() const {
  DCHECK(focus_test_view_);
  return GetBoundsInRoot(focus_test_view_->button_1_->GetBoundsInScreen(),
                         focus_test_view_);
}

gfx::Rect MagnifierFocusTestHelper::GetSecondButtonBoundsInRoot() const {
  DCHECK(focus_test_view_);
  return GetBoundsInRoot(focus_test_view_->button_2_->GetBoundsInScreen(),
                         focus_test_view_);
}

////////////////////////////////////////////////////////////////////////////////
// MagnifierTextInputTestHelper:

void MagnifierTextInputTestHelper::CreateAndShowTextInputView(
    const gfx::Rect& bounds) {
  CreateAndShowTextInputViewInRoot(bounds, Shell::GetPrimaryRootWindow());
}

void MagnifierTextInputTestHelper::CreateAndShowTextInputViewInRoot(
    const gfx::Rect& bounds,
    aura::Window* root) {
  text_input_view_ = new TestTextInputView;
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      text_input_view_, root, bounds);
  widget->Show();
}

gfx::Rect MagnifierTextInputTestHelper::GetTextInputViewBounds() {
  DCHECK(text_input_view_);
  gfx::Rect bounds = text_input_view_->bounds();
  gfx::Point origin = bounds.origin();
  // Convert origin to screen coordinates.
  views::View::ConvertPointToScreen(text_input_view_, &origin);
  // Convert origin to root window coordinates.
  ::wm::ConvertPointFromScreen(GetViewRootWindow(text_input_view_), &origin);
  return gfx::Rect(origin.x(), origin.y(), bounds.width(), bounds.height());
}

gfx::Rect MagnifierTextInputTestHelper::GetCaretBounds() {
  return GetBoundsInRoot(
      GetInputMethod()->GetTextInputClient()->GetCaretBounds(),
      text_input_view_);
}

void MagnifierTextInputTestHelper::FocusOnTextInputView() {
  DCHECK(text_input_view_);
  text_input_view_->FocusOnTextInput();
}

void MagnifierTextInputTestHelper::MaximizeWidget() {
  DCHECK(text_input_view_);
  text_input_view_->GetWidget()->Maximize();
}

ui::InputMethod* MagnifierTextInputTestHelper::GetInputMethod() {
  DCHECK(text_input_view_);
  return text_input_view_->GetWidget()->GetInputMethod();
}

}  // namespace ash
