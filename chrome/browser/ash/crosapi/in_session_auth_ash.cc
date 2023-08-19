// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/in_session_auth_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
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
                                    const absl::optional<std::string>& prompt,
                                    RequestTokenCallback callback) {
  ash::Shell::Get()->in_session_auth_dialog_controller()->ShowAuthDialog(
      ToAshReason(reason),
      base::BindOnce(&InSessionAuthAsh::OnAuthComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthAsh::CheckToken(mojom::Reason reason,
                                  const std::string& token,
                                  CheckTokenCallback callback) {
  bool token_valid;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    token_valid = ash::AuthSessionStorage::Get()->IsValid(token);
  } else {
    auto account_id =
        ash::Shell::Get()->session_controller()->GetActiveAccountId();

    ash::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        ash::quick_unlock::QuickUnlockFactory::GetForAccountId(account_id);
    const ash::quick_unlock::AuthToken* auth_token =
        quick_unlock_storage->GetAuthToken();
    token_valid =
        auth_token != nullptr && auth_token->GetAge().has_value() &&
        token == auth_token->Identifier() &&
        auth_token->GetAge() <= ash::quick_unlock::AuthToken::kTokenExpiration;
  }

  std::move(callback).Run(token_valid);
}

void InSessionAuthAsh::InvalidateToken(const std::string& token) {
  if (ash::features::ShouldUseAuthSessionStorage()) {
    ash::AuthSessionStorage::Get()->Invalidate(token, base::DoNothing());
  } else {
    NOTIMPLEMENTED();
  }
}

void InSessionAuthAsh::OnAuthComplete(RequestTokenCallback callback,
                                      bool success,
                                      const ash::AuthProofToken& token,
                                      base::TimeDelta timeout) {
  std::move(callback).Run(
      success ? mojom::RequestTokenReply::New(token, timeout) : nullptr);
}

}  // namespace crosapi
