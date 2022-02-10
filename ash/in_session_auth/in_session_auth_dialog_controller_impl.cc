// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/in_session_auth/webauthn_request_registrar_impl.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "components/user_manager/known_user.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

InSessionAuthDialogControllerImpl::InSessionAuthDialogControllerImpl()
    : webauthn_request_registrar_(
          std::make_unique<WebAuthnRequestRegistrarImpl>()) {}

InSessionAuthDialogControllerImpl::~InSessionAuthDialogControllerImpl() =
    default;

void InSessionAuthDialogControllerImpl::SetClient(
    InSessionAuthDialogClient* client) {
  client_ = client;
}

void InSessionAuthDialogControllerImpl::ShowAuthenticationDialog(
    aura::Window* source_window,
    const std::string& origin_name,
    FinishCallback finish_callback) {
  DCHECK(client_);
  // Concurrent requests are not supported.
  DCHECK(!dialog_);

  source_window_tracker_.Add(source_window);
  finish_callback_ = std::move(finish_callback);

  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  // GAIA password option is not offered.
  uint32_t auth_methods = AuthDialogContentsView::kAuthPassword;

  if (client_->IsFingerprintAuthAvailable(account_id)) {
    client_->StartFingerprintAuthSession(
        account_id,
        base::BindOnce(
            &InSessionAuthDialogControllerImpl::OnStartFingerprintAuthSession,
            weak_factory_.GetWeakPtr(), account_id, auth_methods, source_window,
            origin_name));
    // OnStartFingerprintAuthSession checks PIN availability.
    return;
  }

  client_->CheckPinAuthAvailability(
      account_id,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), auth_methods, source_window,
                     origin_name));
}

void InSessionAuthDialogControllerImpl::OnStartFingerprintAuthSession(
    AccountId account_id,
    uint32_t auth_methods,
    aura::Window* source_window,
    const std::string& origin_name,
    bool success) {
  if (success)
    auth_methods |= AuthDialogContentsView::kAuthFingerprint;

  client_->CheckPinAuthAvailability(
      account_id,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), auth_methods, source_window,
                     origin_name));
}

void InSessionAuthDialogControllerImpl::OnPinCanAuthenticate(
    uint32_t auth_methods,
    aura::Window* source_window,
    const std::string& origin_name,
    bool pin_auth_available) {
  if (pin_auth_available)
    auth_methods |= AuthDialogContentsView::kAuthPin;

  if (auth_methods == 0) {
    // If neither fingerprint nor PIN is available, we shouldn't receive the
    // request.
    LOG(ERROR) << "Neither fingerprint nor PIN is available.";
    Cancel();
    return;
  }

  if (!source_window_tracker_.Contains(source_window)) {
    LOG(ERROR) << "Source window is no longer available.";
    Cancel();
    return;
  }

  Shell* shell = Shell::Get();
  AccountId account_id = shell->session_controller()->GetActiveAccountId();
  const UserSession* session =
      shell->session_controller()->GetUserSessionByAccountId(account_id);
  DCHECK(session);
  UserAvatar avatar = session->user_info.avatar;

  // TODO(b/156258540): move UserSelectionScreen::BuildAshUserAvatarForUser to
  // somewhere that UserToUserSession could call, to support animated avatars.

  AuthDialogContentsView::AuthMethodsMetadata auth_metadata;
  auth_metadata.autosubmit_pin_length =
      user_manager::KnownUser(shell->local_state())
          .GetUserPinLength(account_id);
  source_window_tracker_.Remove(source_window);
  dialog_ = std::make_unique<InSessionAuthDialog>(
      auth_methods, source_window, origin_name, auth_metadata, avatar);
}

void InSessionAuthDialogControllerImpl::DestroyAuthenticationDialog() {
  DCHECK(client_);
  if (!dialog_)
    return;

  if (dialog_->GetAuthMethods() & AuthDialogContentsView::kAuthFingerprint)
    client_->EndFingerprintAuthSession();

  dialog_.reset();
  source_window_tracker_.RemoveAll();
}

void InSessionAuthDialogControllerImpl::AuthenticateUserWithPasswordOrPin(
    const std::string& password,
    bool authenticated_by_pin,
    OnAuthenticateCallback callback) {
  DCHECK(client_);

  // TODO(b/156258540): Check that PIN is enabled / set up for this user.
  if (authenticated_by_pin &&
      !base::ContainsOnlyChars(password, "0123456789")) {
    OnAuthenticateComplete(std::move(callback), false);
    return;
  }

  client_->AuthenticateUserWithPasswordOrPin(
      password, authenticated_by_pin,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthDialogControllerImpl::AuthenticateUserWithFingerprint(
    base::OnceCallback<void(bool, FingerprintState)> views_callback) {
  DCHECK(client_);

  client_->AuthenticateUserWithFingerprint(base::BindOnce(
      &InSessionAuthDialogControllerImpl::OnFingerprintAuthComplete,
      weak_factory_.GetWeakPtr(), std::move(views_callback)));
}

void InSessionAuthDialogControllerImpl::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  std::move(callback).Run(success);
  if (success)
    OnAuthSuccess();
}

void InSessionAuthDialogControllerImpl::OnFingerprintAuthComplete(
    base::OnceCallback<void(bool, FingerprintState)> views_callback,
    bool success,
    FingerprintState fingerprint_state) {
  // If success is false and retry is allowed, the view will start another
  // fingerprint check.
  std::move(views_callback).Run(success, fingerprint_state);

  if (success)
    OnAuthSuccess();
}

void InSessionAuthDialogControllerImpl::OnAuthSuccess() {
  DestroyAuthenticationDialog();
  if (finish_callback_)
    std::move(finish_callback_).Run(true);
}

void InSessionAuthDialogControllerImpl::Cancel() {
  DestroyAuthenticationDialog();
  if (finish_callback_)
    std::move(finish_callback_).Run(false);
}

void InSessionAuthDialogControllerImpl::OpenInSessionAuthHelpPage() {
  DCHECK(client_);
  client_->OpenInSessionAuthHelpPage();
}

void InSessionAuthDialogControllerImpl::CheckAvailability(
    FinishCallback on_availability_checked) const {
  // Assumes the requests are for the active user (no teleported window).
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();

  if (client_->IsFingerprintAuthAvailable(account_id)) {
    std::move(on_availability_checked).Run(true);
    return;
  }

  client_->CheckPinAuthAvailability(account_id,
                                    std::move(on_availability_checked));
}

}  // namespace ash
