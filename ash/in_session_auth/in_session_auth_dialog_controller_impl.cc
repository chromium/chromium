// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/in_session_auth/authentication_dialog.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel_event_dispatcher.h"
#include "chromeos/ash/components/auth_panel/impl/factor_auth_view_factory.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {
namespace {

std::unique_ptr<views::Widget> CreateAuthDialogWidget(
    std::unique_ptr<views::View> contents_view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = nullptr;
  params.name = "AuthDialogWidget";

  params.delegate->SetInitiallyFocusedView(contents_view.get());
  params.delegate->SetModalType(ui::MODAL_TYPE_SYSTEM);
  params.delegate->SetOwnedByWidget(true);

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  widget->SetContentsView(std::move(contents_view));
  return widget;
}

// TODO(b/271248452): Subscribe to primary display changes, so that the
// authentication dialog correctly changes its location to center on new
// primary displays. We will need to also listen to `work_area` changes and
// reposition the dialog accordingly when that changes.
void CenterWidgetOnPrimaryDisplay(views::Widget* widget) {
  auto bounds = display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  bounds.ClampToCenteredSize(widget->GetContentsView()->GetPreferredSize());
  widget->SetBounds(bounds);
}

}  // namespace

InSessionAuthDialogControllerImpl::InSessionAuthDialogControllerImpl() =
    default;

InSessionAuthDialogControllerImpl::~InSessionAuthDialogControllerImpl() =
    default;

void InSessionAuthDialogControllerImpl::ShowAuthDialog(
    Reason reason,
    auth_panel::AuthCompletionCallback on_auth_complete) {
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  DCHECK(account_id.is_valid());
  DCHECK_NE(auth_token_provider_, nullptr);

  if (reason == Reason::kAccessPasswordManager &&
      features::IsUseAuthPanelInPasswordManagerEnabled()) {
    auto auth_panel = std::make_unique<AuthPanel>(
        std::make_unique<FactorAuthViewFactory>(),
        std::make_unique<AuthFactorStoreFactory>(),
        std::make_unique<AuthPanelEventDispatcherFactory>(),
        std::move(on_auth_complete));
    auth_panel->InitializeUi(AuthFactorsSet{AshAuthFactor::kGaiaPassword},
                             nullptr);

    dialog_ = CreateAuthDialogWidget(std::move(auth_panel));

    CenterWidgetOnPrimaryDisplay(dialog_.get());
    dialog_->Show();
  } else {
    // We don't manage the lifetime of `AuthenticationDialog` here.
    // `AuthenticatonDialog` is-a View and it is instead owned by it's widget,
    // which would properly delete it when the widget is closed.
    (new AuthenticationDialog(
         std::move(on_auth_complete), auth_token_provider_,
         std::make_unique<AuthPerformer>(UserDataAuthClient::Get()),
         account_id))
        ->Show();
  }
}

void InSessionAuthDialogControllerImpl::SetTokenProvider(
    InSessionAuthTokenProvider* auth_token_provider) {
  auth_token_provider_ = auth_token_provider;
}

}  // namespace ash
