// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/cred_man_controller.h"

#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"

namespace password_manager {

CredManController::CredManController(PasswordManagerClient* client)
    : client_(client) {}

CredManController::~CredManController() = default;

bool CredManController::Show(PasswordManagerDriver* driver,
                             bool is_webauthn_form) {
  if (!is_webauthn_form ||
      !webauthn::WebAuthnCredManDelegate::IsCredManEnabled()) {
    return false;
  }
  webauthn::WebAuthnCredManDelegate* cred_man_delegate =
      client_->GetWebAuthnCredManDelegateForDriver(driver);
  // webauthn forms without passkeys should show TouchToFill bottom sheet.
  if (cred_man_delegate->HasResults()) {
    // TODO(crbug/1434278): Avoid using KeyboardReplacingSurfaceClosed.
    cred_man_delegate->SetRequestCompletionCallback(base::BindRepeating(
        [](base::WeakPtr<password_manager::PasswordManagerDriver> driver,
           bool success) {
          driver->KeyboardReplacingSurfaceClosed(
              PasswordManagerDriver::ToShowVirtualKeyboard(!success));
        },
        driver->AsWeakPtr()));
    cred_man_delegate->TriggerFullRequest();
    return true;
  }
  return false;
}

}  // namespace password_manager
