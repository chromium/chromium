// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_util_chromeos.h"

#include "base/notreached.h"
#include "components/password_manager/core/browser/password_access_authenticator.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/in_session_auth.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace password_manager_util_chromeos {

#if BUILDFLAG(IS_CHROMEOS_ASH)
void OnAuthComplete(
    password_manager::PasswordAccessAuthenticator::AuthResultCallback callback,
    bool success,
    const base::UnguessableToken& token,
    base::TimeDelta timeout) {
  // Here we simply ignore `token` and `timeout`, as password manager manages
  // its own auth timeout
  std::move(callback).Run(success);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void OnRequestToken(
    password_manager::PasswordAccessAuthenticator::AuthResultCallback callback,
    crosapi::mojom::RequestTokenReplyPtr reply) {
  // Similarly to `OnAuthComplete`, we ignore the token provided in reply, if
  // any.
  std::move(callback).Run(
      reply != mojo::StructPtr<crosapi::mojom::RequestTokenReply>(nullptr));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void AuthenticateUser(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::InSessionAuthDialogController::Get()->ShowAuthDialog(
      ash::InSessionAuthDialogController::Reason::kAccessPasswordManager,
      base::BindOnce(&OnAuthComplete, std::move(callback)));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (auto* lacros_service = chromeos::LacrosService::Get();
      lacros_service->IsAvailable<crosapi::mojom::InSessionAuth>()) {
    lacros_service->GetRemote<crosapi::mojom::InSessionAuth>()->RequestToken(
        crosapi::mojom::Reason::kAccessPasswordManager,
        base::BindOnce(&OnRequestToken, std::move(callback)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace password_manager_util_chromeos
