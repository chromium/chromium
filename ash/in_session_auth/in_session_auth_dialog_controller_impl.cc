// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/in_session_auth/authentication_dialog.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"

namespace ash {

void InSessionAuthDialogControllerImpl::ShowAuthDialog(
    Reason reason,
    OnAuthComplete on_auth_complete) {
  auto account_id = Shell::Get()->session_controller()->GetActiveAccountId();
  DCHECK(account_id.is_valid());
  DCHECK_NE(auth_token_provider_, nullptr);

  // We don't manage the lifetime of `AuthenticationDialog` here.
  // `AuthenticatonDialog` is-a View and it is instead owned by it's widget,
  // which would properly delete it when the widget is closed.
  (new AuthenticationDialog(
       std::move(on_auth_complete), auth_token_provider_,
       std::make_unique<AuthPerformer>(UserDataAuthClient::Get()), account_id))
      ->Show();
}

void InSessionAuthDialogControllerImpl::SetTokenProvider(
    InSessionAuthTokenProvider* auth_token_provider) {
  auth_token_provider_ = auth_token_provider;
}

}  // namespace ash
