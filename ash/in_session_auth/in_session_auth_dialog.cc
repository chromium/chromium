// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

// The top inset value is set such that the dialog overlaps with the browser
// address bar, for anti-spoofing.
constexpr int kTopInsetDp = 36;

}  // namespace

InSessionAuthDialog::InSessionAuthDialog(
    uint32_t auth_methods,
    aura::Window* parent_window,
    const std::string& origin_name,
    const AuthDialogContentsView::AuthMethodsMetadata& auth_metadata,
    const UserAvatar& avatar)
    : auth_methods_(auth_methods) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::mojom::WindowShowState::kNormal;
  params.parent = parent_window;
  params.name = "AuthDialogWidget";

  auto contents_view = std::make_unique<AuthDialogContentsView>(
      auth_methods, origin_name, auth_metadata, avatar);
  params.delegate->SetInitiallyFocusedView(contents_view.get());
  params.delegate->SetModalType(ui::mojom::ModalType::kNone);
  params.delegate->SetOwnedByWidget(
      views::WidgetDelegate::OwnedByWidgetPassKey());

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget_->SetContentsView(std::move(contents_view));

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
