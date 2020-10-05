// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog.h"

#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/public/cpp/rounded_corner_decorator.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

// The initial height does nothing except determining the vertical position of
// the dialog, since the dialog is centered with the initial height.
constexpr int kCornerRadius = 12;

std::unique_ptr<views::Widget> CreateAuthDialogWidget(
    std::unique_ptr<views::View> contents_view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = nullptr;
  params.name = "AuthDialogWidget";
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = 3;

  params.delegate->SetInitiallyFocusedView(contents_view.get());
  params.delegate->SetModalType(ui::MODAL_TYPE_SYSTEM);
  params.delegate->SetOwnedByWidget(true);

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

}  // namespace

InSessionAuthDialog::InSessionAuthDialog(uint32_t auth_methods)
    : auth_methods_(auth_methods) {
  widget_ = CreateAuthDialogWidget(
      std::make_unique<AuthDialogContentsView>(auth_methods));

  aura::Window* window = widget_->GetNativeWindow();
  rounded_corner_decorator_ = std::make_unique<RoundedCornerDecorator>(
      window, window, window->layer(), kCornerRadius);

  widget_->Show();
}

InSessionAuthDialog::~InSessionAuthDialog() = default;

uint32_t InSessionAuthDialog::GetAuthMethods() const {
  return auth_methods_;
}

}  // namespace ash
