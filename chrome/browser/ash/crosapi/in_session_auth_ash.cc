// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/in_session_auth_ash.h"

#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chromeos/crosapi/mojom/in_session_auth.mojom.h"

namespace crosapi {

ash::InSessionAuthDialogController::Reason ToAshReason(mojom::Reason reason) {
  switch (reason) {
    case mojom::Reason::kAccessPasswordManager:
      return ash::InSessionAuthDialogController::kAccessPasswordManager;
    case mojom::Reason::kModifyAuthFactors:
      return ash::InSessionAuthDialogController::kModifyAuthFactors;
    case mojom::Reason::kModifyAuthFactorsMultidevice:
      return ash::InSessionAuthDialogController::kModifyAuthFactorsMultidevice;
  }
}

InSessionAuthAsh::InSessionAuthAsh() = default;
InSessionAuthAsh::~InSessionAuthAsh() = default;

void InSessionAuthAsh::BindReceiver(
    mojo::PendingReceiver<InSessionAuth> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void InSessionAuthAsh::RequestToken(mojom::Reason reason,
                                    RequestTokenCallback callback) {
  ash::Shell::Get()->in_session_auth_dialog_controller()->ShowAuthDialog(
      ToAshReason(reason),
      base::BindOnce(&InSessionAuthAsh::OnAuthComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthAsh::CheckToken(mojom::Reason reason,
                                  const std::string& token,
                                  CheckTokenCallback callback) {
  // TODO(b/239681292): Use AuthSession-backed tokens.
  auto account_id =
      ash::Shell::Get()->session_controller()->GetActiveAccountId();

  ash::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
  const ash::quick_unlock::AuthToken* auth_token =
      quick_unlock_storage->GetAuthToken();

  std::move(callback).Run(
      auth_token != nullptr && auth_token->GetAge().has_value() &&
      auth_token->GetAge() <= ash::quick_unlock::AuthToken::kTokenExpiration);
}

void InSessionAuthAsh::InvalidateToken(const std::string& token) {
  // TODO(b/239681292): Implement as a feature of AuthSession-backed tokens
  NOTIMPLEMENTED();
}

void InSessionAuthAsh::OnAuthComplete(RequestTokenCallback callback,
                                      bool success,
                                      const base::UnguessableToken& token,
                                      base::TimeDelta timeout) {
  std::move(callback).Run(
      success ? mojom::RequestTokenReply::New(token.ToString(), timeout)
              : nullptr);
}

}  // namespace crosapi
