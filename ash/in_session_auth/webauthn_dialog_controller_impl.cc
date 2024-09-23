// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/webauthn_dialog_controller_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/in_session_auth/in_session_auth_dialog.h"
#include "ash/in_session_auth/webauthn_request_registrar_impl.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/user_manager/known_user.h"
#include "ui/aura/window.h"

namespace ash {

WebAuthNDialogControllerImpl::WebAuthNDialogControllerImpl()
    : webauthn_request_registrar_(
          std::make_unique<WebAuthnRequestRegistrarImpl>()) {}

WebAuthNDialogControllerImpl::~WebAuthNDialogControllerImpl() = default;

void WebAuthNDialogControllerImpl::SetClient(
    InSessionAuthDialogClient* client) {
  client_ = client;
}

void WebAuthNDialogControllerImpl::ShowAuthenticationDialog(
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

  base::OnceClosure continuation =
      base::BindOnce(&WebAuthNDialogControllerImpl::CheckAuthFactorAvailability,
                     weak_factory_.GetWeakPtr(), account_id, origin_name,
                     auth_methods, source_window);

  auto on_auth_session_started = [](base::OnceClosure continuation,
                                    bool is_auth_session_started) {
    if (!is_auth_session_started) {
      LOG(ERROR)
          << "Failed to start cryptohome auth session, exiting dialog early";
      return;
    }
    std::move(continuation).Run();
  };

  client_->StartAuthSession(
      base::BindOnce(on_auth_session_started, std::move(continuation)));
  return;
}

void WebAuthNDialogControllerImpl::CheckAuthFactorAvailability(
    const AccountId& account_id,
    const std::string& origin_name,
    uint32_t auth_methods,
    aura::Window* source_window) {
  if (client_->IsFingerprintAuthAvailable(account_id)) {
    client_->StartFingerprintAuthSession(
        account_id,
        base::BindOnce(
            &WebAuthNDialogControllerImpl::OnStartFingerprintAuthSession,
            weak_factory_.GetWeakPtr(), account_id, auth_methods, source_window,
            origin_name));
    // OnStartFingerprintAuthSession checks PIN availability.
    return;
  }

  client_->CheckPinAuthAvailability(
      account_id,
      base::BindOnce(&WebAuthNDialogControllerImpl::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), auth_methods, source_window,
                     origin_name));
}

void WebAuthNDialogControllerImpl::OnStartFingerprintAuthSession(
    AccountId account_id,
    uint32_t auth_methods,
    aura::Window* source_window,
    const std::string& origin_name,
    bool success) {
  if (success)
    auth_methods |= AuthDialogContentsView::kAuthFingerprint;

  client_->CheckPinAuthAvailability(
      account_id,
      base::BindOnce(&WebAuthNDialogControllerImpl::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), auth_methods, source_window,
                     origin_name));
}

void WebAuthNDialogControllerImpl::OnPinCanAuthenticate(
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

void WebAuthNDialogControllerImpl::DestroyAuthenticationDialog() {
  DCHECK(client_);
  if (!dialog_)
    return;

  if (dialog_->GetAuthMethods() & AuthDialogContentsView::kAuthFingerprint) {
    client_->EndFingerprintAuthSession(
        base::BindOnce(&WebAuthNDialogControllerImpl::ProcessFinalCleanups,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  ProcessFinalCleanups();
}

void WebAuthNDialogControllerImpl::ProcessFinalCleanups() {
  client_->InvalidateAuthSession();
  dialog_.reset();
  source_window_tracker_.RemoveAll();
}

void WebAuthNDialogControllerImpl::AuthenticateUserWithPasswordOrPin(
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
      base::BindOnce(&WebAuthNDialogControllerImpl::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAuthNDialogControllerImpl::AuthenticateUserWithFingerprint(
    base::OnceCallback<void(bool, FingerprintState)> views_callback) {
  DCHECK(client_);

  client_->AuthenticateUserWithFingerprint(
      base::BindOnce(&WebAuthNDialogControllerImpl::OnFingerprintAuthComplete,
                     weak_factory_.GetWeakPtr(), std::move(views_callback)));
}

void WebAuthNDialogControllerImpl::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  if (success) {
    std::move(callback).Run(/*success=*/true, /*can_use_pin=*/true);
    OnAuthSuccess();
    return;
  }

  // PIN might be locked out after an unsuccessful authentication, check if it's
  // still available so the UI can be updated.
  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  client_->CheckPinAuthAvailability(
      account_id, base::BindOnce(std::move(callback), /*success=*/false));
}

void WebAuthNDialogControllerImpl::OnFingerprintAuthComplete(
    base::OnceCallback<void(bool, FingerprintState)> views_callback,
    bool success,
    FingerprintState fingerprint_state) {
  // If success is false and retry is allowed, the view will start another
  // fingerprint check.
  std::move(views_callback).Run(success, fingerprint_state);

  if (success)
    OnAuthSuccess();
}

void WebAuthNDialogControllerImpl::OnAuthSuccess() {
  DestroyAuthenticationDialog();
  if (finish_callback_)
    std::move(finish_callback_).Run(true);
}

void WebAuthNDialogControllerImpl::Cancel() {
  DestroyAuthenticationDialog();
  if (finish_callback_)
    std::move(finish_callback_).Run(false);
}

void WebAuthNDialogControllerImpl::OpenInSessionAuthHelpPage() {
  DCHECK(client_);
  client_->OpenInSessionAuthHelpPage();
}

void WebAuthNDialogControllerImpl::CheckAvailability(
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
