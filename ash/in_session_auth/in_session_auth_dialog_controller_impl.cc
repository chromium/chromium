// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/in_session_auth_dialog_controller_impl.h"

#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/public/cpp/in_session_auth_dialog_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"

namespace ash {

InSessionAuthDialogControllerImpl::InSessionAuthDialogControllerImpl() =
    default;

InSessionAuthDialogControllerImpl::~InSessionAuthDialogControllerImpl() =
    default;

void InSessionAuthDialogControllerImpl::SetClient(
    InSessionAuthDialogClient* client) {
  client_ = client;
}

void InSessionAuthDialogControllerImpl::ShowAuthenticationDialog(
    FinishCallback finish_callback) {
  DCHECK(client_);
  // Concurrent requests are not supported.
  DCHECK(!dialog_);

  finish_callback_ = std::move(finish_callback);

  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  // GAIA password option is not offered.
  uint32_t auth_methods = 0;

  if (client_->IsFingerprintAuthAvailable(account_id)) {
    client_->StartFingerprintAuthSession(
        account_id,
        base::BindOnce(
            &InSessionAuthDialogControllerImpl::OnStartFingerprintAuthSession,
            weak_factory_.GetWeakPtr(), account_id, auth_methods));
    // OnStartFingerprintAuthSession checks PIN availability.
    return;
  }

  client_->CheckPinAuthAvailability(
      account_id,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), auth_methods));
}

void InSessionAuthDialogControllerImpl::OnStartFingerprintAuthSession(
    AccountId account_id,
    uint32_t auth_methods,
    bool success) {
  if (success)
    auth_methods |= AuthDialogContentsView::kAuthFingerprint;

  client_->CheckPinAuthAvailability(
      account_id,
      base::BindOnce(&InSessionAuthDialogControllerImpl::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), auth_methods));
}

void InSessionAuthDialogControllerImpl::OnPinCanAuthenticate(
    uint32_t auth_methods,
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

  dialog_ = std::make_unique<InSessionAuthDialog>(auth_methods);
}

void InSessionAuthDialogControllerImpl::DestroyAuthenticationDialog() {
  DCHECK(client_);
  if (!dialog_)
    return;

  if (dialog_->GetAuthMethods() & AuthDialogContentsView::kAuthFingerprint)
    client_->EndFingerprintAuthSession();

  dialog_.reset();
}

void InSessionAuthDialogControllerImpl::AuthenticateUserWithPin(
    const std::string& pin,
    OnAuthenticateCallback callback) {
  DCHECK(client_);

  // TODO(b/156258540): Check that PIN is enabled / set up for this user.

  if (!base::ContainsOnlyChars(pin, "0123456789")) {
    OnAuthenticateComplete(std::move(callback), false);
    return;
  }

  client_->AuthenticateUserWithPasswordOrPin(
      pin, true,
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
  // TODO(b/156258540): Manage retry instead of closing directly.
  DestroyAuthenticationDialog();
  if (finish_callback_)
    std::move(finish_callback_).Run(success);
}

void InSessionAuthDialogControllerImpl::OnFingerprintAuthComplete(
    base::OnceCallback<void(bool, FingerprintState)> views_callback,
    bool success,
    FingerprintState fingerprint_state) {
  // If success is false and retry is allowed, the view will start another
  // fingerprint check.
  std::move(views_callback).Run(success, fingerprint_state);

  if (success) {
    DestroyAuthenticationDialog();
    if (finish_callback_)
      std::move(finish_callback_).Run(success);
    return;
  }
}

void InSessionAuthDialogControllerImpl::Cancel() {
  DestroyAuthenticationDialog();
  if (finish_callback_)
    std::move(finish_callback_).Run(false);
}

}  // namespace ash
