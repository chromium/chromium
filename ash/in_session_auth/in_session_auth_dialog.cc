// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog.h"

#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

// The top inset value is set such that the dialog overlaps with the browser
// address bar, for anti-spoofing.
constexpr int kTopInsetDp = 36;

std::unique_ptr<views::Widget> CreateAuthDialogWidget(
    std::unique_ptr<views::View> contents_view,
    aura::Window* parent) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = parent;
  params.name = "AuthDialogWidget";

  params.delegate->SetInitiallyFocusedView(contents_view.get());
  params.delegate->SetModalType(ui::MODAL_TYPE_NONE);
  params.delegate->SetOwnedByWidget(true);

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

}  // namespace

InSessionAuthDialog::InSessionAuthDialog(
    uint32_t auth_methods,
    aura::Window* parent_window,
    const std::string& origin_name,
    const AuthDialogContentsView::AuthMethodsMetadata& auth_metadata,
    const UserAvatar& avatar)
    : auth_methods_(auth_methods) {
  widget_ = CreateAuthDialogWidget(
      std::make_unique<AuthDialogContentsView>(auth_methods, origin_name,
                                               auth_metadata, avatar),
      parent_window);
  gfx::Rect bounds = parent_window->GetBoundsInScreen();
  gfx::Size preferred_size = widget_->GetContentsView()->GetPreferredSize();
  int horizontal_inset_dp = (bounds.width() - preferred_size.width()) / 2;
  int bottom_inset_dp = bounds.height() - kTopInsetDp - preferred_size.height();
  bounds.Inset(gfx::Insets::TLBR(kTopInsetDp, horizontal_inset_dp,
                                 bottom_inset_dp, horizontal_inset_dp));
  widget_->SetBounds(bounds);

  widget_->Show();
}

InSessionAuthDialog::~InSessionAuthDialog() = default;

uint32_t InSessionAuthDialog::GetAuthMethods() const {
  return auth_methods_;
}

}  // namespace ash
