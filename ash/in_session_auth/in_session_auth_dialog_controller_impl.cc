// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/in_session_auth/authentication_dialog.h"
#include "ash/in_session_auth/in_session_auth_dialog_contents_view.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/auth_panel/impl/auth_factor_store.h"
#include "chromeos/ash/components/auth_panel/impl/auth_panel.h"
#include "chromeos/ash/components/auth_panel/public/shared_types.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/osauth/impl/legacy_auth_surface_registry.h"
#include "chromeos/ash/components/osauth/impl/request/password_manager_auth_request.h"
#include "chromeos/ash/components/osauth/impl/request/settings_auth_request.h"
#include "chromeos/ash/components/osauth/public/auth_factor_status_consumer.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/components/webauthn/webauthn_request_registrar.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

AuthPurpose InSessionAuthReasonToAuthPurpose(
    InSessionAuthDialogController::Reason reason) {
  switch (reason) {
    case InSessionAuthDialogController::Reason::kAccessPasswordManager:
    case InSessionAuthDialogController::Reason::kAccessMultideviceSettings:
      return AuthPurpose::kUserVerification;
    case InSessionAuthDialogController::Reason::kAccessAuthenticationSettings:
      return AuthPurpose::kAuthSettings;
  }
}

std::unique_ptr<views::Widget> CreateAuthDialogWidget(
    std::unique_ptr<views::View> contents_view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::mojom::WindowShowState::kNormal;
  params.parent = nullptr;
  params.name = "AuthDialogWidget";

  params.delegate->SetInitiallyFocusedView(contents_view.get());
  params.delegate->SetModalType(ui::mojom::ModalType::kSystem);
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

void InSessionAuthDialogControllerImpl::CreateAndShowAuthPanel(
    const std::optional<std::string>& prompt,
    auth_panel::AuthCompletionCallback on_auth_complete,
    Reason reason,
    const AccountId& account_id) {
  state_ = State::kShowing;
  on_auth_complete_ = std::move(on_auth_complete);
  prompt_ = prompt;

  auto* auth_hub = AuthHub::Get();

  auto continuation = base::BindOnce(
      &AuthHub::StartAuthentication, base::Unretained(auth_hub), account_id,
      InSessionAuthReasonToAuthPurpose(reason), this);

  auth_hub->EnsureInitialized(std::move(continuation));
}

void InSessionAuthDialogControllerImpl::ShowAuthDialog(
    Reason reason,
    const std::optional<std::string>& prompt,
    auth_panel::AuthCompletionCallback on_auth_complete) {
  if (state_ != State::kNotShown) {
    LOG(ERROR) << "Trying to show authentication dialog in session while "
                  "another is currently active, returning";
    std::move(on_auth_complete)
        .Run(false, ash::AuthProofToken{}, base::TimeDelta{});
    return;
  }

  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  DCHECK(account_id.is_valid());
  DCHECK_NE(auth_token_provider_, nullptr);

  if (reason == Reason::kAccessPasswordManager &&
      features::IsUseAuthPanelInSessionEnabled()) {
    Shell::Get()->active_session_auth_controller()->ShowAuthDialog(
        std::make_unique<PasswordManagerAuthRequest>(
            base::UTF8ToUTF16(prompt.value_or("")),
            std::move(on_auth_complete)));
  } else if (reason == Reason::kAccessAuthenticationSettings &&
             features::IsUseAuthPanelInSessionEnabled()) {
    Shell::Get()->active_session_auth_controller()->ShowAuthDialog(
        std::make_unique<SettingsAuthRequest>(std::move(on_auth_complete)));
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

void InSessionAuthDialogControllerImpl::ShowLegacyWebAuthnDialog(
    const std::string& rp_id,
    const std::string& window_id,
    WebAuthNDialogController::FinishCallback on_auth_complete) {
  aura::Window* source_window =
      chromeos::webauthn::WebAuthnRequestRegistrar::Get()
          ->GetWindowForRequestId(window_id);
  if (!source_window) {
    LOG(ERROR) << "Can't find the window for the given window id.";
    std::move(on_auth_complete).Run(false);
    return;
  }
  WebAuthNDialogController::Get()->ShowAuthenticationDialog(
      source_window, rp_id, std::move(on_auth_complete));
}

void InSessionAuthDialogControllerImpl::SetTokenProvider(
    InSessionAuthTokenProvider* auth_token_provider) {
  auth_token_provider_ = auth_token_provider;
}

void InSessionAuthDialogControllerImpl::OnUserAuthAttemptRejected() {
  NOTIMPLEMENTED();
}

void InSessionAuthDialogControllerImpl::OnUserAuthAttemptConfirmed(
    AuthHubConnector* connector,
    raw_ptr<AuthFactorStatusConsumer>& out_consumer) {
  CHECK_EQ(state_, State::kShowing);
  CHECK_EQ(contents_view_, nullptr);

  auto contents_view = std::make_unique<InSessionAuthDialogContentsView>(
      prompt_,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnEndAuthentication,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &InSessionAuthDialogControllerImpl::OnAuthPanelPreferredSizeChanged,
          weak_factory_.GetWeakPtr()),
      connector, AuthHub::Get());

  contents_view_ = contents_view.get();
  out_consumer = contents_view->GetAuthPanel();
  dialog_ = CreateAuthDialogWidget(std::move(contents_view));
  dialog_->Show();
  state_ = State::kShown;
  AuthParts::Get()
      ->GetLegacyAuthSurfaceRegistry()
      ->NotifyInSessionAuthDialogShown(connector);
}

void InSessionAuthDialogControllerImpl::OnAuthPanelPreferredSizeChanged() {
  CenterWidgetOnPrimaryDisplay(dialog_.get());
}

void InSessionAuthDialogControllerImpl::OnAccountNotFound() {
  NOTIMPLEMENTED();
}

void InSessionAuthDialogControllerImpl::OnUserAuthAttemptCancelled() {
  NotifyFailure();
  OnEndAuthentication();
}

void InSessionAuthDialogControllerImpl::OnFactorAttemptFailed(
    AshAuthFactor factor) {
  contents_view_->ShowAuthError(factor);
}

void InSessionAuthDialogControllerImpl::NotifySuccess(
    const AuthProofToken& token) {
  if (!on_auth_complete_) {
    LOG(ERROR) << "Encountered null auth completion callback, possible double "
                  "invocation?";
    return;
  }

  std::move(on_auth_complete_)
      .Run(true, token, cryptohome::kAuthsessionInitialLifetime);
}

void InSessionAuthDialogControllerImpl::NotifyFailure() {
  if (on_auth_complete_) {
    std::move(on_auth_complete_)
        .Run(false, /*token=*/{},
             /*timeout=*/{});
  }
}

void InSessionAuthDialogControllerImpl::OnUserAuthSuccess(
    AshAuthFactor factor,
    const AuthProofToken& token) {
  NotifySuccess(token);
}

void InSessionAuthDialogControllerImpl::OnEndAuthentication() {
  contents_view_ = nullptr;
  dialog_.reset();
  state_ = State::kNotShown;
}

}  // namespace ash
