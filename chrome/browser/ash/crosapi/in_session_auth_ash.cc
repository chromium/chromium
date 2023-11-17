// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/in_session_auth_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"

namespace crosapi {

ash::InSessionAuthDialogController::Reason ToAshReason(
    chromeos::auth::mojom::Reason reason) {
  switch (reason) {
    case chromeos::auth::mojom::Reason::kAccessPasswordManager:
      return ash::InSessionAuthDialogController::kAccessPasswordManager;
    case chromeos::auth::mojom::Reason::kModifyAuthFactors:
      return ash::InSessionAuthDialogController::kModifyAuthFactors;
    case chromeos::auth::mojom::Reason::kModifyAuthFactorsMultidevice:
      return ash::InSessionAuthDialogController::kModifyAuthFactorsMultidevice;
  }
}

InSessionAuthAsh::InSessionAuthAsh() = default;
InSessionAuthAsh::~InSessionAuthAsh() = default;

void InSessionAuthAsh::BindReceiver(
    mojo::PendingReceiver<InSessionAuth> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void InSessionAuthAsh::RequestToken(chromeos::auth::mojom::Reason reason,
                                    const absl::optional<std::string>& prompt,
                                    RequestTokenCallback callback) {
  ash::Shell::Get()->in_session_auth_dialog_controller()->ShowAuthDialog(
      ToAshReason(reason),
      base::BindOnce(&InSessionAuthAsh::OnAuthComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void InSessionAuthAsh::CheckToken(chromeos::auth::mojom::Reason reason,
                                  const std::string& token,
                                  CheckTokenCallback callback) {
  bool token_valid;
  token_valid = ash::AuthSessionStorage::Get()->IsValid(token);

  std::move(callback).Run(token_valid);
}

void InSessionAuthAsh::InvalidateToken(const std::string& token) {
  ash::AuthSessionStorage::Get()->Invalidate(token, base::DoNothing());
}

void InSessionAuthAsh::OnAuthComplete(RequestTokenCallback callback,
                                      bool success,
                                      const ash::AuthProofToken& token,
                                      base::TimeDelta timeout) {
  std::move(callback).Run(
      success ? chromeos::auth::mojom::RequestTokenReply::New(token, timeout)
              : nullptr);
}

}  // namespace crosapi
