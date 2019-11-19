// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test_child_modal_parent.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

using views::Widget;

namespace ash {

namespace {

// Parent window layout.
const int kWindowLeft = 170;
const int kWindowTop = 200;
const int kWindowWidth = 400;
const int kWindowHeight = 400;
const int kButtonHeight = 35;

// Child window size.
const int kChildWindowWidth = 330;
const int kChildWindowHeight = 200;

// Child window layout.
const int kTextfieldLeft = 10;
const int kTextfieldTop = 20;
const int kTextfieldWidth = 300;
const int kTextfieldHeight = 35;

const SkColor kModalParentColor = SK_ColorBLUE;
const SkColor kChildColor = SK_ColorWHITE;

}  // namespace

class ChildModalWindow : public views::WidgetDelegateView {
 public:
  ChildModalWindow() {
    SetBackground(views::CreateSolidBackground(kChildColor));
    views::Textfield* modal_child_textfield = new views::Textfield;
    AddChildView(modal_child_textfield);
    modal_child_textfield->SetBounds(kTextfieldLeft, kTextfieldTop,
                                     kTextfieldWidth, kTextfieldHeight);
    modal_child_textfield->SetPlaceholderText(
        base::ASCIIToUTF16("modal child window"));
  }
  ~ChildModalWindow() override = default;

 private:
  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kChildWindowWidth, kChildWindowHeight);
  }

  // Overridden from WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    return base::ASCIIToUTF16("Examples: Child Modal Window");
  }
  ui::ModalType GetModalType() const override { return ui::MODAL_TYPE_CHILD; }

  DISALLOW_COPY_AND_ASSIGN(ChildModalWindow);
};

// static
TestChildModalParent* TestChildModalParent::Show(aura::Window* context) {
  auto* test_child_modal_parent = new TestChildModalParent(context);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.delegate = test_child_modal_parent;
  params.context = context;
  params.bounds =
      gfx::Rect(kWindowLeft, kWindowTop, kWindowWidth, kWindowHeight);
  widget->Init(std::move(params));
  widget->Show();
  return test_child_modal_parent;
}

TestChildModalParent::TestChildModalParent(aura::Window* context)
    : modal_parent_(std::make_unique<Widget>()),
      textfield_(new views::Textfield),
      host_(new views::NativeViewHost) {
  textfield_->SetPlaceholderText(base::ASCIIToUTF16("top level window"));
  Widget::InitParams params(Widget::InitParams::TYPE_CONTROL);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = context;
  modal_parent_->Init(std::move(params));
  modal_parent_->GetRootView()->SetBackground(
      views::CreateSolidBackground(kModalParentColor));
  auto* modal_parent_textfield = new views::Textfield;
  modal_parent_->GetRootView()->AddChildView(modal_parent_textfield);
  modal_parent_textfield->SetBounds(kTextfieldLeft, kTextfieldTop,
                                    kTextfieldWidth, kTextfieldHeight);
  modal_parent_textfield->SetPlaceholderText(
      base::ASCIIToUTF16("modal parent window"));
  modal_parent_->GetNativeView()->SetName("ModalParent");
  auto button = views::MdTextButton::Create(
      this, base::ASCIIToUTF16("Show/Hide Child Modal Window"));
  button_ = AddChildView(std::move(button));
  AddChildView(textfield_);
  AddChildView(host_);
}

TestChildModalParent::~TestChildModalParent() = default;

aura::Window* TestChildModalParent::GetModalParent() const {
  return modal_parent_->GetNativeView();
}

aura::Window* TestChildModalParent::ShowModalChild() {
  DCHECK(!modal_child_);
  modal_child_ = Widget::CreateWindowWithParent(new ChildModalWindow,
                                                GetWidget()->GetNativeView());
  wm::SetModalParent(modal_child_->GetNativeView(),
                     modal_parent_->GetNativeView());
  modal_child_->AddObserver(this);
  modal_child_->GetNativeView()->SetName("ChildModalWindow");
  modal_child_->Show();
  return modal_child_->GetNativeView();
}

base::string16 TestChildModalParent::GetWindowTitle() const {
  return base::ASCIIToUTF16("Examples: Child Modal Parent");
}

void TestChildModalParent::Layout() {
  int running_y = y();
  button_->SetBounds(x(), running_y, width(), kButtonHeight);
  running_y += kButtonHeight;
  textfield_->SetBounds(x(), running_y, width(), kTextfieldHeight);
  running_y += kTextfieldHeight;
  host_->SetBounds(x(), running_y, width(), height() - running_y);
}

void TestChildModalParent::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    host_->Attach(modal_parent_->GetNativeView());
    GetWidget()->GetNativeView()->SetName("Parent");
  }
}

void TestChildModalParent::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  DCHECK_EQ(sender, button_);
  if (!modal_child_)
    ShowModalChild();
  else
    modal_child_->Close();
}

void TestChildModalParent::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(modal_child_, widget);
  modal_child_ = nullptr;
}

}  // namespace ash
