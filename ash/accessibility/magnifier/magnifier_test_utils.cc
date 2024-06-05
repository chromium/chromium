// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/magnifier_test_utils.h"

#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/input_method.h"
#include "ui/views/accessibility/view_accessibility.h"
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
// TestTextInputView:

// A view that contains a single text field for testing text input events.
class TestTextInputView : public views::WidgetDelegateView {
 public:
  TestTextInputView() : text_field_(new views::Textfield) {
    text_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_TEXT);
    std::string name = "Hello, world";
    text_field_->GetViewAccessibility().SetName(base::UTF8ToUTF16(name));
    AddChildView(text_field_.get());
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }
  TestTextInputView(const TestTextInputView&) = delete;
  TestTextInputView& operator=(const TestTextInputView&) = delete;
  ~TestTextInputView() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(50, 50);
  }

  void FocusOnTextInput() { text_field_->RequestFocus(); }

 private:
  raw_ptr<views::Textfield> text_field_;  // owned by views hierarchy.
};

////////////////////////////////////////////////////////////////////////////////
// MagnifierFocusTestHelper:

// static
constexpr int MagnifierFocusTestHelper::kButtonHeight;

// static
constexpr gfx::Size MagnifierFocusTestHelper::kTestFocusViewSize;

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
  views::Widget* widget =
      views::Widget::CreateWindowWithContext(text_input_view_, root, bounds);
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
