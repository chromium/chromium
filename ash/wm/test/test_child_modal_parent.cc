// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/test/test_child_modal_parent.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
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

views::WidgetDelegateView* CreateChildModalWindow() {
  auto child = std::make_unique<views::WidgetDelegateView>();
  child->SetModalType(ui::mojom::ModalType::kChild);
  child->SetTitle(u"Examples: Child Modal Window");
  child->SetBackground(views::CreateSolidBackground(kChildColor));
  child->SetPreferredSize(gfx::Size(kChildWindowWidth, kChildWindowHeight));

  auto textfield = std::make_unique<views::Textfield>();
  textfield->SetBounds(kTextfieldLeft, kTextfieldTop, kTextfieldWidth,
                       kTextfieldHeight);
  textfield->SetPlaceholderText(u"modal child window");
  child->AddChildView(std::move(textfield));
  return child.release();
}

}  // namespace

// static
TestChildModalParent* TestChildModalParent::Show(aura::Window* context) {
  auto* test_child_modal_parent = new TestChildModalParent(context);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
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
  SetTitle(u"Examples: Child Modal Parent");
  textfield_->SetPlaceholderText(u"top level window");
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_CONTROL);
  params.context = context;
  modal_parent_->Init(std::move(params));
  modal_parent_->GetRootView()->SetBackground(
      views::CreateSolidBackground(kModalParentColor));
  auto* modal_parent_textfield = new views::Textfield;
  modal_parent_->GetRootView()->AddChildView(modal_parent_textfield);
  modal_parent_textfield->SetBounds(kTextfieldLeft, kTextfieldTop,
                                    kTextfieldWidth, kTextfieldHeight);
  modal_parent_textfield->SetPlaceholderText(u"modal parent window");
  modal_parent_->GetNativeView()->SetName("ModalParent");
  auto button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(&TestChildModalParent::ButtonPressed,
                          base::Unretained(this)),
      u"Show/Hide Child Modal Window");
  button_ = AddChildView(std::move(button));
  AddChildView(textfield_.get());
  AddChildView(host_.get());
}

TestChildModalParent::~TestChildModalParent() = default;

aura::Window* TestChildModalParent::GetModalParent() {
  return modal_parent_->GetNativeView();
}

aura::Window* TestChildModalParent::ShowModalChild() {
  DCHECK(!modal_child_);
  modal_child_ = Widget::CreateWindowWithParent(CreateChildModalWindow(),
                                                GetWidget()->GetNativeView());
  wm::SetModalParent(modal_child_->GetNativeView(),
                     modal_parent_->GetNativeView());
  modal_child_->AddObserver(this);
  modal_child_->GetNativeView()->SetName("ChildModalWindow");
  modal_child_->Show();
  return modal_child_->GetNativeView();
}

void TestChildModalParent::Layout(PassKey) {
  int running_y = y();
  button_->SetBounds(x(), running_y, width(), kButtonHeight);
  running_y += kButtonHeight;
  textfield_->SetBounds(x(), running_y, width(), kTextfieldHeight);
  running_y += kTextfieldHeight;
  host_->SetBounds(x(), running_y, width(), height() - running_y);
}

void TestChildModalParent::AddedToWidget() {
  // The function requires a Widget be present.
  DCHECK(GetWidget());
  host_->Attach(modal_parent_->GetNativeView());
  GetWidget()->GetNativeView()->SetName("Parent");
}

void TestChildModalParent::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(modal_child_, widget);
  modal_child_ = nullptr;
}

void TestChildModalParent::ButtonPressed() {
  if (modal_child_)
    modal_child_->Close();
  else
    ShowModalChild();
}

}  // namespace ash
