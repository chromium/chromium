// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// Default window position.
const int kWindowLeft = 170;
const int kWindowTop = 200;

// Default window size.
const int kWindowWidth = 400;
const int kWindowHeight = 400;

// A window showing samples of commonly used widgets.
class WidgetsWindow : public views::WidgetDelegateView {
 public:
  WidgetsWindow();
  ~WidgetsWindow() override;

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;
  bool CanResize() const override;

 private:
  views::LabelButton* button_;
  views::LabelButton* disabled_button_;
  views::Checkbox* checkbox_;
  views::Checkbox* checkbox_disabled_;
  views::Checkbox* checkbox_checked_;
  views::Checkbox* checkbox_checked_disabled_;
  views::RadioButton* radio_button_;
  views::RadioButton* radio_button_disabled_;
  views::RadioButton* radio_button_selected_;
  views::RadioButton* radio_button_selected_disabled_;
};

WidgetsWindow::WidgetsWindow()
    : checkbox_(new views::Checkbox(base::ASCIIToUTF16("Checkbox"))),
      checkbox_disabled_(
          new views::Checkbox(base::ASCIIToUTF16("Checkbox disabled"))),
      checkbox_checked_(
          new views::Checkbox(base::ASCIIToUTF16("Checkbox checked"))),
      checkbox_checked_disabled_(
          new views::Checkbox(base::ASCIIToUTF16("Checkbox checked disabled"))),
      radio_button_(
          new views::RadioButton(base::ASCIIToUTF16("Radio button"), 0)),
      radio_button_disabled_(
          new views::RadioButton(base::ASCIIToUTF16("Radio button disabled"),
                                 0)),
      radio_button_selected_(
          new views::RadioButton(base::ASCIIToUTF16("Radio button selected"),
                                 0)),
      radio_button_selected_disabled_(new views::RadioButton(
          base::ASCIIToUTF16("Radio button selected disabled"),
          1)) {
  button_ = AddChildView(
      views::MdTextButton::Create(nullptr, base::ASCIIToUTF16("Button")));
  disabled_button_ = AddChildView(views::MdTextButton::Create(
      nullptr, base::ASCIIToUTF16("Disabled button")));
  disabled_button_->SetEnabled(false);
  AddChildView(checkbox_);
  checkbox_disabled_->SetEnabled(false);
  AddChildView(checkbox_disabled_);
  checkbox_checked_->SetChecked(true);
  AddChildView(checkbox_checked_);
  checkbox_checked_disabled_->SetChecked(true);
  checkbox_checked_disabled_->SetEnabled(false);
  AddChildView(checkbox_checked_disabled_);
  AddChildView(radio_button_);
  radio_button_disabled_->SetEnabled(false);
  AddChildView(radio_button_disabled_);
  radio_button_selected_->SetChecked(true);
  AddChildView(radio_button_selected_);
  radio_button_selected_disabled_->SetChecked(true);
  radio_button_selected_disabled_->SetEnabled(false);
  AddChildView(radio_button_selected_disabled_);
}

WidgetsWindow::~WidgetsWindow() = default;

void WidgetsWindow::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetLocalBounds(), SK_ColorWHITE);
}

void WidgetsWindow::Layout() {
  const int kVerticalPad = 5;
  int left = 5;
  int top = kVerticalPad;
  for (auto* view : children()) {
    gfx::Size preferred = view->GetPreferredSize();
    view->SetBounds(left, top, preferred.width(), preferred.height());
    top += preferred.height() + kVerticalPad;
  }
}

gfx::Size WidgetsWindow::CalculatePreferredSize() const {
  return gfx::Size(kWindowWidth, kWindowHeight);
}

base::string16 WidgetsWindow::GetWindowTitle() const {
  return base::ASCIIToUTF16("Examples: Widgets");
}

bool WidgetsWindow::CanResize() const {
  return true;
}

}  // namespace

namespace ash {
namespace shell {

void CreateWidgetsWindow() {
  gfx::Rect bounds(kWindowLeft, kWindowTop, kWindowWidth, kWindowHeight);
  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      new WidgetsWindow, Shell::GetPrimaryRootWindow(), bounds);
  widget->GetNativeView()->SetName("WidgetsWindow");
  widget->Show();
}

}  // namespace shell
}  // namespace ash
