// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cred_man_controller.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_credential_filler.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"

namespace password_manager {

CredManController::CredManController() = default;

CredManController::~CredManController() = default;

bool CredManController::Show(
    raw_ptr<webauthn::WebAuthnCredManDelegate> cred_man_delegate,
    std::unique_ptr<PasswordCredentialFiller> filler,
    bool is_webauthn_form) {
  if (!filler) {
    return false;
  }
  if (!cred_man_delegate || !is_webauthn_form ||
      !webauthn::WebAuthnCredManDelegate::IsCredManEnabled() ||
      !cred_man_delegate->HasResults()) {
    filler->CleanUp(PasswordManagerDriver::ToShowVirtualKeyboard(false));
    return false;
  }
  filler_ = std::move(filler);
  // webauthn forms without passkeys should show TouchToFill bottom sheet.
  cred_man_delegate->SetRequestCompletionCallback(base::BindRepeating(
      [](base::WeakPtr<password_manager::PasswordCredentialFiller> filler,
         bool success) {
        if (!filler || !filler->IsReadyToFill()) {
          return;
        }
        filler->CleanUp(PasswordManagerDriver::ToShowVirtualKeyboard(!success));
      },
      filler_->AsWeakPtr()));
  cred_man_delegate->SetFillingCallback(
      base::BindOnce(&PasswordCredentialFiller::FillUsernameAndPassword,
                     filler_->AsWeakPtr()));
  cred_man_delegate->TriggerFullRequest();
  return true;
}

}  // namespace password_manager
